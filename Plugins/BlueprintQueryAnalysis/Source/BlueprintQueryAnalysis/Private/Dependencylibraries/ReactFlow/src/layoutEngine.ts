// External graph-layout engines.
//
//  * RuntimeFlow / NodeFlow  -> ELK "layered" (Sugiyama) layout via elkjs.
//    elkjs' bundled build runs its solver on an in-process fake worker, so it
//    works inside the offline single-file (file://) Unreal WebBrowser with no
//    external worker file or blob URL.
//  * StaticDependency / Folder -> a deterministic d3-force simulation that
//    spreads dependency clusters into a dynamic force-directed layout.
//
// Both engines return absolute, top-left node positions keyed by node id. When
// a layout is applied the graph is "flattened": lane / annotation containers are
// dropped and child nodes lose their parent offset, so the engine owns the
// final coordinates.

import ELK, { type ElkNode } from 'elkjs/lib/elk.bundled.js';
import {
  forceCenter,
  forceCollide,
  forceLink,
  forceManyBody,
  forceSimulation,
  forceX,
  forceY,
  type SimulationLinkDatum,
  type SimulationNodeDatum,
} from 'd3-force';
import type { BPQAAnyNode, BPQAFlowEdge, BPQAViewMode } from './types';

export type PositionMap = Record<string, { x: number; y: number }>;

export interface LayoutResult {
  positions: PositionMap;
  /** When true the caller should flatten lane/annotation containers. */
  flatten: boolean;
  engine: 'elk' | 'd3' | 'none';
}

const DEFAULT_NODE_WIDTH = 236;
const DEFAULT_NODE_HEIGHT = 94;

const elk = new ELK();

function readNodeWidth(node: BPQAAnyNode): number {
  const width = node.style?.width;
  return typeof width === 'number' ? width : DEFAULT_NODE_WIDTH;
}

function readNodeHeight(node: BPQAAnyNode): number {
  const height = node.style?.height ?? node.style?.minHeight;
  return typeof height === 'number' ? height : DEFAULT_NODE_HEIGHT;
}

function layoutNodes(nodes: BPQAAnyNode[]): BPQAAnyNode[] {
  // Only real graph nodes participate; lane / annotation scaffolding is dropped.
  return nodes.filter((node) => node.type === 'bpqaNode' && !node.hidden);
}

export function isElkMode(mode: BPQAViewMode): boolean {
  return mode === 'RuntimeFlow' || mode === 'NodeFlow';
}

export function isForceMode(mode: BPQAViewMode): boolean {
  return mode === 'StaticDependency' || mode === 'FolderDependency' || mode === 'FolderAssetDependency';
}

export async function computeLayout(
  mode: BPQAViewMode,
  nodes: BPQAAnyNode[],
  edges: BPQAFlowEdge[],
): Promise<LayoutResult> {
  const participating = layoutNodes(nodes);
  if (participating.length === 0) {
    return { positions: {}, flatten: false, engine: 'none' };
  }

  if (isElkMode(mode)) {
    return computeElkLayout(participating, edges);
  }
  if (isForceMode(mode)) {
    return computeForceLayout(participating, edges);
  }
  return { positions: {}, flatten: false, engine: 'none' };
}

async function computeElkLayout(nodes: BPQAAnyNode[], edges: BPQAFlowEdge[]): Promise<LayoutResult> {
  const idSet = new Set(nodes.map((node) => node.id));
  const children: ElkNode[] = nodes.map((node) => ({
    id: node.id,
    width: readNodeWidth(node),
    height: readNodeHeight(node),
  }));

  const elkEdges = edges
    .filter((edge) => idSet.has(edge.source) && idSet.has(edge.target) && !edge.hidden)
    .map((edge) => ({ id: edge.id, sources: [edge.source], targets: [edge.target] }));

  const graph: ElkNode = {
    id: 'root',
    layoutOptions: {
      'elk.algorithm': 'layered',
      'elk.direction': 'DOWN',
      'elk.edgeRouting': 'ORTHOGONAL',
      'elk.layered.spacing.nodeNodeBetweenLayers': '96',
      'elk.spacing.nodeNode': '64',
      'elk.layered.nodePlacement.strategy': 'BRANDES_KOEPF',
      'elk.layered.considerModelOrder.strategy': 'NODES_AND_EDGES',
      'elk.layered.crossingMinimization.semiInteractive': 'true',
    },
    children,
    edges: elkEdges,
  };

  try {
    const result = await elk.layout(graph);
    const positions: PositionMap = {};
    for (const child of result.children ?? []) {
      if (typeof child.x === 'number' && typeof child.y === 'number') {
        positions[child.id] = { x: child.x, y: child.y };
      }
    }
    return { positions, flatten: true, engine: 'elk' };
  } catch {
    return { positions: {}, flatten: false, engine: 'none' };
  }
}

interface ForceNode extends SimulationNodeDatum {
  id: string;
  width: number;
  height: number;
}

const FORCE_TICKS = 320;

function computeForceLayout(nodes: BPQAAnyNode[], edges: BPQAFlowEdge[]): LayoutResult {
  const idSet = new Set(nodes.map((node) => node.id));
  // Seed positions from the incoming layout so the simulation is deterministic
  // and converges quickly instead of exploding from a random start.
  const simNodes: ForceNode[] = nodes.map((node) => ({
    id: node.id,
    width: readNodeWidth(node),
    height: readNodeHeight(node),
    x: node.position.x + readNodeWidth(node) / 2,
    y: node.position.y + readNodeHeight(node) / 2,
  }));

  const simLinks: SimulationLinkDatum<ForceNode>[] = edges
    .filter((edge) => idSet.has(edge.source) && idSet.has(edge.target) && !edge.hidden)
    .map((edge) => ({ source: edge.source, target: edge.target }));

  const radius = (node: ForceNode): number => Math.hypot(node.width, node.height) / 2 + 18;

  const simulation = forceSimulation<ForceNode>(simNodes)
    .force('charge', forceManyBody<ForceNode>().strength(-680).distanceMax(1400))
    .force(
      'link',
      forceLink<ForceNode, SimulationLinkDatum<ForceNode>>(simLinks)
        .id((node) => node.id)
        .distance(240)
        .strength(0.45),
    )
    .force('collide', forceCollide<ForceNode>().radius(radius).strength(0.9))
    .force('center', forceCenter(0, 0))
    .force('x', forceX(0).strength(0.04))
    .force('y', forceY(0).strength(0.04))
    .stop();

  simulation.tick(FORCE_TICKS);

  const positions: PositionMap = {};
  for (const node of simNodes) {
    positions[node.id] = {
      x: (node.x ?? 0) - node.width / 2,
      y: (node.y ?? 0) - node.height / 2,
    };
  }
  return { positions, flatten: true, engine: 'd3' };
}
