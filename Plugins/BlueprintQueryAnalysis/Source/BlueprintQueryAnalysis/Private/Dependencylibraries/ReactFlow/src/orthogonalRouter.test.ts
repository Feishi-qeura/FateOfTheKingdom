import { describe, expect, it } from 'vitest';
import {
  pathMidpoint,
  pointsToSvgPath,
  routeOrthogonal,
  simplify,
  type Point,
  type Rect,
} from './orthogonalRouter';

function isOrthogonal(points: Point[]): boolean {
  for (let index = 0; index < points.length - 1; index += 1) {
    const a = points[index];
    const b = points[index + 1];
    const horizontal = Math.abs(a.y - b.y) < 0.5;
    const vertical = Math.abs(a.x - b.x) < 0.5;
    if (!horizontal && !vertical) {
      return false;
    }
  }
  return true;
}

function crossesInterior(points: Point[], rect: Rect, margin: number): boolean {
  const left = rect.x - margin;
  const right = rect.x + rect.width + margin;
  const top = rect.y - margin;
  const bottom = rect.y + rect.height + margin;
  for (let index = 0; index < points.length - 1; index += 1) {
    const a = points[index];
    const b = points[index + 1];
    if (Math.abs(a.y - b.y) < 0.5) {
      const y = a.y;
      if (y > top + 0.5 && y < bottom - 0.5 && Math.min(a.x, b.x) < right - 0.5 && Math.max(a.x, b.x) > left + 0.5) {
        return true;
      }
    } else if (Math.abs(a.x - b.x) < 0.5) {
      const x = a.x;
      if (x > left + 0.5 && x < right - 0.5 && Math.min(a.y, b.y) < bottom - 0.5 && Math.max(a.y, b.y) > top + 0.5) {
        return true;
      }
    }
  }
  return false;
}

describe('routeOrthogonal', () => {
  it('produces an orthogonal path between two points with no obstacles', () => {
    const points = routeOrthogonal(
      { source: { x: 0, y: 0 }, target: { x: 200, y: 200 }, sourceSide: 'bottom', targetSide: 'top' },
      [],
    );
    expect(points.length).toBeGreaterThanOrEqual(2);
    expect(points[0]).toEqual({ x: 0, y: 0 });
    expect(points[points.length - 1]).toEqual({ x: 200, y: 200 });
    expect(isOrthogonal(points)).toBe(true);
  });

  it('routes around an obstacle sitting between source and target', () => {
    const obstacle: Rect = { x: 60, y: 80, width: 80, height: 40 };
    const points = routeOrthogonal(
      { source: { x: 100, y: 0 }, target: { x: 100, y: 200 }, sourceSide: 'bottom', targetSide: 'top' },
      [obstacle],
      { margin: 20 },
    );
    expect(isOrthogonal(points)).toBe(true);
    expect(crossesInterior(points, obstacle, 20)).toBe(false);
  });

  it('leaves the source and enters the target perpendicular to the node edge', () => {
    // Source/target are horizontally offset; a naive A* could approach the target
    // from the side (a horizontal final segment -> sideways arrow). The route must
    // still leave straight down from the source and arrive straight down into the
    // target so the arrow head points square into the node.
    const points = routeOrthogonal(
      { source: { x: 40, y: 0 }, target: { x: 300, y: 240 }, sourceSide: 'bottom', targetSide: 'top' },
      [],
    );
    expect(isOrthogonal(points)).toBe(true);

    const first = points[0];
    const second = points[1];
    expect(Math.abs(first.x - second.x)).toBeLessThan(0.5); // first segment vertical

    const last = points[points.length - 1];
    const penultimate = points[points.length - 2];
    expect(Math.abs(last.x - penultimate.x)).toBeLessThan(0.5); // last segment vertical
  });

  it('keeps endpoints exactly at the requested anchor points', () => {
    const points = routeOrthogonal(
      { source: { x: 12, y: 34 }, target: { x: 256, y: 410 }, sourceSide: 'bottom', targetSide: 'top' },
      [{ x: 100, y: 100, width: 60, height: 60 }],
    );
    expect(points[0]).toEqual({ x: 12, y: 34 });
    expect(points[points.length - 1]).toEqual({ x: 256, y: 410 });
  });
});

describe('simplify', () => {
  it('collapses collinear runs into a single segment', () => {
    const simplified = simplify([
      { x: 0, y: 0 },
      { x: 0, y: 10 },
      { x: 0, y: 20 },
      { x: 30, y: 20 },
    ]);
    expect(simplified).toEqual([
      { x: 0, y: 0 },
      { x: 0, y: 20 },
      { x: 30, y: 20 },
    ]);
  });
});

describe('pointsToSvgPath', () => {
  it('emits an M/L command sequence', () => {
    expect(pointsToSvgPath([
      { x: 0, y: 0 },
      { x: 10, y: 0 },
      { x: 10, y: 10 },
    ])).toBe('M0,0 L10,0 L10,10');
  });
});

describe('pathMidpoint', () => {
  it('returns the geometric midpoint along the poly-line length', () => {
    const mid = pathMidpoint([
      { x: 0, y: 0 },
      { x: 100, y: 0 },
    ]);
    expect(mid).toEqual({ x: 50, y: 0 });
  });
});
