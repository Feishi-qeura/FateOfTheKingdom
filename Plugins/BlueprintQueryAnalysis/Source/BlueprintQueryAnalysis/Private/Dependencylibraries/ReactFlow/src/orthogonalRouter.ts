// Obstacle-avoiding orthogonal connector routing.
//
// Implements the approach described in
// "Routing Orthogonal Diagram Connectors in JavaScript"
// (https://medium.com/swlh/routing-orthogonal-diagram-connectors-in-javascript-191dc2c5ff70):
//
//  1. Derive "rulers" (candidate grid lines) from the two endpoints and from the
//     bounds of every obstacle inflated by a margin.
//  2. Build the cartesian product of unique X / Y rulers to obtain candidate
//     "spots", discarding spots that fall inside an inflated obstacle.
//  3. Connect orthogonally-adjacent spots into a visibility graph, rejecting
//     segments that pass through an obstacle.
//  4. Run A* from the source spot to the destination spot, penalising bends so
//     the routed path prefers long straight runs with few turns.
//  5. Simplify the resulting poly-line by collapsing collinear points.

export interface Point {
  x: number;
  y: number;
}

export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface RouteRequest {
  source: Point;
  target: Point;
  /** Direction the segment should leave the source / enter the target. */
  sourceSide?: 'top' | 'bottom' | 'left' | 'right';
  targetSide?: 'top' | 'bottom' | 'left' | 'right';
}

export interface RouteOptions {
  /** Inflation applied around every obstacle so routes keep clear of nodes. */
  margin?: number;
  /** Extra cost added every time the path changes direction. */
  bendPenalty?: number;
  /** Safety cap on candidate spots; falls back to a simple route past it. */
  maxSpots?: number;
}

const DEFAULT_MARGIN = 26;
const DEFAULT_BEND_PENALTY = 38;
const DEFAULT_MAX_SPOTS = 2600;
const EPSILON = 0.01;

function inflate(rect: Rect, margin: number): Rect {
  return {
    x: rect.x - margin,
    y: rect.y - margin,
    width: rect.width + margin * 2,
    height: rect.height + margin * 2,
  };
}

function rectRight(rect: Rect): number {
  return rect.x + rect.width;
}

function rectBottom(rect: Rect): number {
  return rect.y + rect.height;
}

function pointInsideRect(point: Point, rect: Rect): boolean {
  return (
    point.x > rect.x + EPSILON &&
    point.x < rectRight(rect) - EPSILON &&
    point.y > rect.y + EPSILON &&
    point.y < rectBottom(rect) - EPSILON
  );
}

// Does the axis-aligned segment [a, b] cross the interior of rect?
function segmentCrossesRect(a: Point, b: Point, rect: Rect): boolean {
  const left = rect.x;
  const right = rectRight(rect);
  const top = rect.y;
  const bottom = rectBottom(rect);

  if (Math.abs(a.y - b.y) < EPSILON) {
    // Horizontal segment.
    const y = a.y;
    if (y <= top + EPSILON || y >= bottom - EPSILON) {
      return false;
    }
    const segMin = Math.min(a.x, b.x);
    const segMax = Math.max(a.x, b.x);
    return segMin < right - EPSILON && segMax > left + EPSILON;
  }

  if (Math.abs(a.x - b.x) < EPSILON) {
    // Vertical segment.
    const x = a.x;
    if (x <= left + EPSILON || x >= right - EPSILON) {
      return false;
    }
    const segMin = Math.min(a.y, b.y);
    const segMax = Math.max(a.y, b.y);
    return segMin < bottom - EPSILON && segMax > top + EPSILON;
  }

  return false;
}

function uniqueSorted(values: number[]): number[] {
  const sorted = [...values].sort((left, right) => left - right);
  const result: number[] = [];
  for (const value of sorted) {
    if (result.length === 0 || Math.abs(result[result.length - 1] - value) > 0.5) {
      result.push(value);
    }
  }
  return result;
}

function sideOffset(point: Point, side: RouteRequest['sourceSide'], margin: number): Point {
  switch (side) {
    case 'top':
      return { x: point.x, y: point.y - margin };
    case 'bottom':
      return { x: point.x, y: point.y + margin };
    case 'left':
      return { x: point.x - margin, y: point.y };
    case 'right':
      return { x: point.x + margin, y: point.y };
    default:
      return point;
  }
}

function key(point: Point): string {
  return `${Math.round(point.x)}:${Math.round(point.y)}`;
}

function manhattan(a: Point, b: Point): number {
  return Math.abs(a.x - b.x) + Math.abs(a.y - b.y);
}

function direction(from: Point, to: Point): 'h' | 'v' | 'none' {
  if (Math.abs(from.y - to.y) < EPSILON && Math.abs(from.x - to.x) >= EPSILON) {
    return 'h';
  }
  if (Math.abs(from.x - to.x) < EPSILON && Math.abs(from.y - to.y) >= EPSILON) {
    return 'v';
  }
  return 'none';
}

/**
 * Routes a single orthogonal connector around the supplied obstacles.
 * Returns an ordered list of poly-line points (always at least 2). When no
 * obstacle-free route is found it falls back to a simple two-bend route.
 */
export function routeOrthogonal(
  request: RouteRequest,
  obstacles: Rect[],
  options: RouteOptions = {},
): Point[] {
  const margin = options.margin ?? DEFAULT_MARGIN;
  const bendPenalty = options.bendPenalty ?? DEFAULT_BEND_PENALTY;
  const maxSpots = options.maxSpots ?? DEFAULT_MAX_SPOTS;

  const inflated = obstacles.map((rect) => inflate(rect, margin));

  // Pull the endpoints a little away from their owning node so the path has a
  // clean stub before it starts weaving between obstacles.
  const start = sideOffset(request.source, request.sourceSide, margin);
  const end = sideOffset(request.target, request.targetSide, margin);

  const xs: number[] = [request.source.x, request.target.x, start.x, end.x];
  const ys: number[] = [request.source.y, request.target.y, start.y, end.y];
  for (const rect of inflated) {
    xs.push(rect.x, rectRight(rect), rect.x + rect.width / 2);
    ys.push(rect.y, rectBottom(rect), rect.y + rect.height / 2);
  }

  const rulerX = uniqueSorted(xs);
  const rulerY = uniqueSorted(ys);

  if (rulerX.length * rulerY.length > maxSpots) {
    return fallbackRoute(request);
  }

  // Candidate spots = grid intersections that are not buried inside an obstacle.
  const spots: Point[] = [];
  const spotIndex = new Map<string, number>();
  const blockedFor = (point: Point): boolean => {
    for (const rect of inflated) {
      if (pointInsideRect(point, rect)) {
        // Endpoints sit on their node edge; never discard them.
        if (manhattan(point, request.source) < EPSILON || manhattan(point, request.target) < EPSILON) {
          continue;
        }
        return true;
      }
    }
    return false;
  };

  const pushSpot = (point: Point): number => {
    const id = key(point);
    const existing = spotIndex.get(id);
    if (existing !== undefined) {
      return existing;
    }
    const index = spots.length;
    spots.push(point);
    spotIndex.set(id, index);
    return index;
  };

  for (const x of rulerX) {
    for (const y of rulerY) {
      const point = { x, y };
      if (!blockedFor(point)) {
        pushSpot(point);
      }
    }
  }

  const startIndex = pushSpot(start);
  const endIndex = pushSpot(end);
  pushSpot(request.source);
  pushSpot(request.target);

  // Adjacency: nearest visible neighbour along each of the 4 axis directions.
  const columns = new Map<number, number[]>();
  const rows = new Map<number, number[]>();
  spots.forEach((spot, index) => {
    const cx = Math.round(spot.x);
    const cy = Math.round(spot.y);
    (columns.get(cx) ?? columns.set(cx, []).get(cx)!).push(index);
    (rows.get(cy) ?? rows.set(cy, []).get(cy)!).push(index);
  });

  const segmentClear = (a: Point, b: Point): boolean => {
    for (const rect of inflated) {
      if (segmentCrossesRect(a, b, rect)) {
        return false;
      }
    }
    return true;
  };

  const neighbors = (index: number): number[] => {
    const result: number[] = [];
    const spot = spots[index];
    const colIndices = columns.get(Math.round(spot.x)) ?? [];
    const rowIndices = rows.get(Math.round(spot.y)) ?? [];

    // Closest reachable neighbour above and below within the same column.
    let up = -1;
    let down = -1;
    for (const other of colIndices) {
      if (other === index) {
        continue;
      }
      const candidate = spots[other];
      if (candidate.y < spot.y && (up === -1 || candidate.y > spots[up].y)) {
        up = other;
      } else if (candidate.y > spot.y && (down === -1 || candidate.y < spots[down].y)) {
        down = other;
      }
    }
    let left = -1;
    let right = -1;
    for (const other of rowIndices) {
      if (other === index) {
        continue;
      }
      const candidate = spots[other];
      if (candidate.x < spot.x && (left === -1 || candidate.x > spots[left].x)) {
        left = other;
      } else if (candidate.x > spot.x && (right === -1 || candidate.x < spots[right].x)) {
        right = other;
      }
    }
    for (const candidate of [up, down, left, right]) {
      if (candidate !== -1 && segmentClear(spot, spots[candidate])) {
        result.push(candidate);
      }
    }
    return result;
  };

  // Route between the two *stubs* (not the raw anchors). The stub of each side
  // sits one margin straight off the node edge, so bracketing the routed path
  // with the real anchors guarantees the first and last segments run
  // perpendicular to the node — the arrow head then enters the target square-on
  // instead of sliding in horizontally along the node's side.
  const path = aStar(spots, startIndex, endIndex, neighbors, bendPenalty, startIndex, endIndex);
  if (!path) {
    return fallbackRoute(request);
  }

  return simplify([request.source, ...path.map((index) => spots[index]), request.target]);
}

function aStar(
  spots: Point[],
  source: number,
  target: number,
  neighbors: (index: number) => number[],
  bendPenalty: number,
  preferStart: number,
  preferEnd: number,
): number[] | null {
  const goal = spots[target];
  const gScore = new Map<number, number>();
  const cameFrom = new Map<number, number>();
  const open = new Set<number>([source]);
  gScore.set(source, 0);

  // Seed the route through the stub spots so it leaves / arrives cleanly.
  void preferStart;
  void preferEnd;

  const fScore = new Map<number, number>();
  fScore.set(source, manhattan(spots[source], goal));

  while (open.size > 0) {
    let current = -1;
    let best = Number.POSITIVE_INFINITY;
    for (const candidate of open) {
      const score = fScore.get(candidate) ?? Number.POSITIVE_INFINITY;
      if (score < best) {
        best = score;
        current = candidate;
      }
    }

    if (current === target) {
      const path = [current];
      let cursor = current;
      while (cameFrom.has(cursor)) {
        cursor = cameFrom.get(cursor)!;
        path.unshift(cursor);
      }
      return path;
    }

    open.delete(current);
    const prev = cameFrom.get(current);
    const incoming = prev !== undefined ? direction(spots[prev], spots[current]) : 'none';

    for (const next of neighbors(current)) {
      const stepDir = direction(spots[current], spots[next]);
      const bend = incoming !== 'none' && stepDir !== incoming ? bendPenalty : 0;
      const tentative = (gScore.get(current) ?? Number.POSITIVE_INFINITY) + manhattan(spots[current], spots[next]) + bend;
      if (tentative < (gScore.get(next) ?? Number.POSITIVE_INFINITY)) {
        cameFrom.set(next, current);
        gScore.set(next, tentative);
        fScore.set(next, tentative + manhattan(spots[next], goal));
        open.add(next);
      }
    }
  }

  return null;
}

// Collapse runs of collinear points so the SVG path only keeps real corners.
export function simplify(points: Point[]): Point[] {
  if (points.length <= 2) {
    return dedupe(points);
  }

  const deduped = dedupe(points);
  const result: Point[] = [deduped[0]];
  for (let index = 1; index < deduped.length - 1; index += 1) {
    const prev = result[result.length - 1];
    const current = deduped[index];
    const next = deduped[index + 1];
    const collinearX = Math.abs(prev.x - current.x) < EPSILON && Math.abs(current.x - next.x) < EPSILON;
    const collinearY = Math.abs(prev.y - current.y) < EPSILON && Math.abs(current.y - next.y) < EPSILON;
    if (!collinearX && !collinearY) {
      result.push(current);
    }
  }
  result.push(deduped[deduped.length - 1]);
  return result;
}

function dedupe(points: Point[]): Point[] {
  const result: Point[] = [];
  for (const point of points) {
    const last = result[result.length - 1];
    if (!last || Math.abs(last.x - point.x) > EPSILON || Math.abs(last.y - point.y) > EPSILON) {
      result.push(point);
    }
  }
  return result;
}

function fallbackRoute(request: RouteRequest): Point[] {
  const { source, target } = request;
  const vertical = request.sourceSide === 'top' || request.sourceSide === 'bottom' || request.targetSide === 'top' || request.targetSide === 'bottom';
  if (vertical) {
    const midY = (source.y + target.y) / 2;
    return simplify([
      source,
      { x: source.x, y: midY },
      { x: target.x, y: midY },
      target,
    ]);
  }
  const midX = (source.x + target.x) / 2;
  return simplify([
    source,
    { x: midX, y: source.y },
    { x: midX, y: target.y },
    target,
  ]);
}

/** Builds an SVG path string from an ordered list of poly-line points. */
export function pointsToSvgPath(points: Point[]): string {
  if (points.length === 0) {
    return '';
  }
  const [first, ...rest] = points;
  return `M${first.x},${first.y}` + rest.map((point) => ` L${point.x},${point.y}`).join('');
}

/** Returns the mid-point of the routed poly-line (for label placement). */
export function pathMidpoint(points: Point[]): Point {
  if (points.length === 0) {
    return { x: 0, y: 0 };
  }
  if (points.length === 1) {
    return points[0];
  }

  let total = 0;
  for (let index = 0; index < points.length - 1; index += 1) {
    total += manhattan(points[index], points[index + 1]);
  }
  let target = total / 2;
  for (let index = 0; index < points.length - 1; index += 1) {
    const segment = manhattan(points[index], points[index + 1]);
    if (target <= segment) {
      const ratio = segment === 0 ? 0 : target / segment;
      return {
        x: points[index].x + (points[index + 1].x - points[index].x) * ratio,
        y: points[index].y + (points[index + 1].y - points[index].y) * ratio,
      };
    }
    target -= segment;
  }
  return points[points.length - 1];
}
