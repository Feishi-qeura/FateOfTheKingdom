// Computes obstacle-avoiding orthogonal routes for every edge, given the final
// node positions. Routes are computed once after a layout settles (and on drag
// stop) rather than every frame, matching the "render once" interaction model.

import { pathMidpoint, pointsToSvgPath, routeOrthogonal, type Point, type Rect } from './orthogonalRouter';
import type { BPQAAnyNode, BPQAFlowEdge } from './types';

export interface EdgeRoute {
  path: string;
  labelX: number;
  labelY: number;
}

const DEFAULT_NODE_WIDTH = 236;
const DEFAULT_NODE_HEIGHT = 94;
const ROUTE_REGION_PADDING = 140;

function nodeWidth(node: BPQAAnyNode): number {
  const width = node.style?.width;
  return typeof width === 'number' ? width : DEFAULT_NODE_WIDTH;
}

function nodeHeight(node: BPQAAnyNode): number {
  const height = node.style?.height ?? node.style?.minHeight;
  return typeof height === 'number' ? height : DEFAULT_NODE_HEIGHT;
}

function nodeRect(node: BPQAAnyNode): Rect {
  return {
    x: node.position.x,
    y: node.position.y,
    width: nodeWidth(node),
    height: nodeHeight(node),
  };
}

function bottomCenter(node: BPQAAnyNode): Point {
  return { x: node.position.x + nodeWidth(node) / 2, y: node.position.y + nodeHeight(node) };
}

function topCenter(node: BPQAAnyNode): Point {
  return { x: node.position.x + nodeWidth(node) / 2, y: node.position.y };
}

function regionContains(region: Rect, rect: Rect): boolean {
  return (
    rect.x + rect.width >= region.x &&
    rect.x <= region.x + region.width &&
    rect.y + rect.height >= region.y &&
    rect.y <= region.y + region.height
  );
}

export function computeOrthogonalRoutes(
  nodes: BPQAAnyNode[],
  edges: BPQAFlowEdge[],
): Record<string, EdgeRoute> {
  const graphNodes = nodes.filter((node) => node.type === 'bpqaNode' && !node.hidden);
  const nodeById = new Map(graphNodes.map((node) => [node.id, node] as const));
  const obstacles = graphNodes.map((node) => ({ id: node.id, rect: nodeRect(node) }));

  const routes: Record<string, EdgeRoute> = {};
  for (const edge of edges) {
    if (edge.hidden) {
      continue;
    }
    const source = nodeById.get(edge.source);
    const target = nodeById.get(edge.target);
    if (!source || !target) {
      continue;
    }

    const start = bottomCenter(source);
    const end = topCenter(target);

    // Limit obstacles to those near the edge so the A* grid stays bounded.
    const region: Rect = {
      x: Math.min(start.x, end.x) - ROUTE_REGION_PADDING,
      y: Math.min(start.y, end.y) - ROUTE_REGION_PADDING,
      width: Math.abs(start.x - end.x) + ROUTE_REGION_PADDING * 2,
      height: Math.abs(start.y - end.y) + ROUTE_REGION_PADDING * 2,
    };

    const localObstacles = obstacles
      .filter((entry) => entry.id !== edge.source && entry.id !== edge.target && regionContains(region, entry.rect))
      .map((entry) => entry.rect);

    const points = routeOrthogonal(
      { source: start, target: end, sourceSide: 'bottom', targetSide: 'top' },
      localObstacles,
    );
    const mid = pathMidpoint(points);
    routes[edge.id] = { path: pointsToSvgPath(points), labelX: mid.x, labelY: mid.y };
  }

  return routes;
}
