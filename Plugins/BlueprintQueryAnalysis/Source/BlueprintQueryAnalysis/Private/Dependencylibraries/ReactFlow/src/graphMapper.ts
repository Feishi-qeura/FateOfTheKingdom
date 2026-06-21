import { Position, type NodeOrigin } from '@xyflow/react';
import { getEdgeAccent, getNodeVisual, isFolderNode, normalizeViewMode } from './styleRules';
import type {
  BPQAAnyNode,
  BPQAFlowEdge,
  BPQAFlowNode,
  BPQAGraphEdge,
  BPQAGraphNode,
  BPQAGraphPayload,
  BPQAMappedGraph,
  BPQAViewMode,
} from './types';

const NODE_WIDTH = 236;
const NODE_HEIGHT = 94;
const STATIC_COLUMN_GAP = 350;
const STATIC_ROW_GAP = 178;
const INACTIVE_COLUMN_GAP = 278;
const INACTIVE_ROW_GAP = 124;
const RUNTIME_NODE_COLUMN_GAP = 236;
const RUNTIME_NODE_ROW_GAP = 146;
const RUNTIME_NODE_STAGGER_Y = 42;
const NODE_ORIGIN: NodeOrigin = [0, 0];
const STATIC_TREE_COLLAPSE_NODE_LIMIT = 96;
const COLLISION_NODE_GAP = 74;
const EDGE_ROUTE_LANE_GAP = 30;
const EDGE_ROUTE_OFFSET_GAP = 26;
const EDGE_SIDE_ROUTE_BASE_OFFSET = 150;

interface BPQAEdgeRouteInfo {
  routeOffset: number;
  routeLane: number;
}

const RUNTIME_LANES = [
  { id: '__runtime_lane_root', depth: 0, title: '树结构层级: Root', x: 0, y: 0, width: 1000, height: 220, columns: 4 },
  { id: '__runtime_lane_trunk', depth: 1, title: '树干结构: TreeTrunk', x: 0, y: 260, width: 1040, height: 450, columns: 4 },
  { id: '__runtime_lane_branch', depth: 2, title: '树枝层级: Branch', x: 0, y: 820, width: 1040, height: 520, columns: 4 },
  { id: '__runtime_lane_static', depth: 3, title: '静态层级 Static: 作用域全局', x: 1180, y: 290, width: 430, height: 980, columns: 1 },
] as const;

export interface BPQAMapGraphOptions {
  collapseLargeStaticTree?: boolean;
}

export function mapGraphToReactFlow(
  graph: BPQAGraphPayload | undefined,
  selectedNodeId = '',
  options: BPQAMapGraphOptions = {},
): BPQAMappedGraph {
  const safeGraph = normalizeGraph(graph);
  const mode = normalizeViewMode(safeGraph.mode);
  const nodes = safeGraph.nodes;
  const edges = safeGraph.edges.filter((edge) => edge.from && edge.to);
  const nodeIds = new Set(nodes.map((node) => node.id));
  const visibleEdges = edges.filter((edge) => nodeIds.has(edge.from) && nodeIds.has(edge.to));
  const connectedIds = makeConnectedIdSet(visibleEdges);
  let flowNodes =
    mode === 'RuntimeFlow'
      ? layoutRuntimeNodes(nodes, visibleEdges, connectedIds, selectedNodeId)
      : layoutStructureNodes(nodes, visibleEdges, connectedIds, selectedNodeId, mode);
  flowNodes = resolveNodeCollisions(flowNodes);
  flowNodes = applyExecutionOrder(flowNodes, mode);
  const routeInfos = makeRouteInfoMap(visibleEdges, flowNodes, mode);
  let flowEdges = visibleEdges.map((edge, index) =>
    mapEdge(edge, index, mode, routeInfos.get(index) ?? { routeOffset: 0, routeLane: 0 }),
  );

  if ((options.collapseLargeStaticTree ?? true) && isStaticCollapseMode(mode)) {
    const collapsed = collapseLargeStaticTree(flowNodes, flowEdges);
    flowNodes = collapsed.nodes;
    flowEdges = collapsed.edges;
  }

  return {
    title: safeGraph.title,
    mode,
    nodes: flowNodes,
    edges: flowEdges,
    sourceNodeCount: nodes.length,
    sourceEdgeCount: visibleEdges.length,
  };
}

export function truncateText(value: string | undefined, maxLength: number): string {
  const safeValue = value?.trim() ?? '';
  if (safeValue.length <= maxLength) {
    return safeValue;
  }

  return `${safeValue.slice(0, Math.max(0, maxLength - 1))}...`;
}

function normalizeGraph(graph: BPQAGraphPayload | undefined): Required<Pick<BPQAGraphPayload, 'title' | 'mode'>> & {
  nodes: BPQAGraphNode[];
  edges: BPQAGraphEdge[];
} {
  return {
    title: graph?.title?.trim() || 'Blueprint Insight',
    mode: normalizeViewMode(graph?.mode),
    nodes: Array.isArray(graph?.nodes) ? graph.nodes.filter((node) => Boolean(node.id)) : [],
    edges: Array.isArray(graph?.edges) ? graph.edges : [],
  };
}

function makeConnectedIdSet(edges: BPQAGraphEdge[]): Set<string> {
  const connectedIds = new Set<string>();
  edges.forEach((edge) => {
    connectedIds.add(edge.from);
    connectedIds.add(edge.to);
  });
  return connectedIds;
}

function mapEdge(edge: BPQAGraphEdge, index: number, mode: BPQAViewMode, routeInfo: BPQAEdgeRouteInfo): BPQAFlowEdge {
  const accent = getEdgeAccent(edge, mode);
  return {
    id: `bpqa-edge-${index}-${stableDomId(edge.from)}-${stableDomId(edge.to)}`,
    type: 'step',
    source: edge.from,
    target: edge.to,
    sourceHandle: 'out',
    targetHandle: 'in',
    label: truncateText(edge.reason || edge.type || '', 34),
    data: {
      bpqa: edge,
      accent,
      glow: accent,
      label: truncateText(edge.reason || edge.type || '', 34),
      viewMode: mode,
      routeOffset: routeInfo.routeOffset,
      routeLane: routeInfo.routeLane,
      shape: edge.shape ?? edge.motionShape,
      motionShape: edge.motionShape ?? edge.shape,
      duration: edge.duration ?? edge.motionDuration,
      motionSpeedSeconds: edge.motionDuration ?? edge.duration,
    },
  };
}

function makeRouteInfoMap(edges: BPQAGraphEdge[], nodes: BPQAAnyNode[], mode: BPQAViewMode): Map<number, BPQAEdgeRouteInfo> {
  const nodesById = new Map(nodes.map((node) => [node.id, node] as const));
  const absolutePositions = makeAbsoluteNodePositionMap(nodes);
  const routeLanes = new Map<string, number>();
  const result = new Map<number, BPQAEdgeRouteInfo>();
  const groups = new Map<string, number[]>();
  const singleEdgeSideRouteOffsets = new Map<number, number>();
  edges.forEach((edge, index) => {
    const sourceNode = nodesById.get(edge.from);
    const targetNode = nodesById.get(edge.to);
    const sourcePosition = sourceNode ? absolutePositions.get(sourceNode.id) ?? sourceNode.position : { x: 0, y: 0 };
    const targetPosition = targetNode ? absolutePositions.get(targetNode.id) ?? targetNode.position : { x: 0, y: 0 };
    const sourceBottom = sourcePosition.y + (sourceNode ? readAnyNodeHeight(sourceNode) : NODE_HEIGHT);
    const targetBottom = targetPosition.y + (targetNode ? readAnyNodeHeight(targetNode) : NODE_HEIGHT);
    const topBand = Math.min(sourcePosition.y, targetPosition.y);
    const bottomBand = Math.max(sourceBottom, targetBottom);
    const key =
      mode === 'RuntimeFlow'
        ? `runtime:${sourceNode?.parentId ?? '__root'}:${targetNode?.parentId ?? '__root'}`
        : `${Math.round(topBand / 8) * 8}:${Math.round(bottomBand / 8) * 8}`;
    const group = groups.get(key) ?? [];
    group.push(index);
    groups.set(key, group);

    const laneIndex = routeLanes.get(key) ?? 0;
    routeLanes.set(key, laneIndex + 1);
    result.set(index, {
      routeOffset: 0,
      routeLane: bottomBand + 48 + laneIndex * EDGE_ROUTE_LANE_GAP,
    });

    if (shouldUseSideRoute(sourceNode, targetNode, sourcePosition, targetPosition)) {
      singleEdgeSideRouteOffsets.set(index, makeSingleEdgeSideRouteOffset(sourcePosition.x, targetPosition.x));
    }
  });

  groups.forEach((indices) => {
    indices.forEach((edgeIndex, localIndex) => {
      const routeInfo = result.get(edgeIndex) ?? { routeOffset: 0, routeLane: 0 };
      const distributedOffset = makeDistributedRouteOffset(localIndex, indices.length, mode);
      result.set(edgeIndex, {
        ...routeInfo,
        routeOffset: distributedOffset || singleEdgeSideRouteOffsets.get(edgeIndex) || 0,
      });
    });
  });
  return result;
}

function layoutStructureNodes(
  nodes: BPQAGraphNode[],
  edges: BPQAGraphEdge[],
  connectedIds: Set<string>,
  selectedNodeId: string,
  mode: BPQAViewMode,
): BPQAAnyNode[] {
  const orphanAssets = nodes.filter((node) => !isFolderNode(node) && !connectedIds.has(node.id));
  const comparator = mode === 'NodeFlow' ? compareNodesByTimeline : compareNodes;

  // Tree area: folders plus connected assets. Folders are not classified by dependency activity.
  const treeNodes = nodes.filter((node) => isFolderNode(node) || connectedIds.has(node.id));
  const result: BPQAAnyNode[] = [];
  let treeRightEdge = 0;

  if (treeNodes.length > 0) {
    const { laidOut, rightEdge } = layoutTree(treeNodes, edges, connectedIds, selectedNodeId, mode, comparator);
    result.push(...laidOut);
    treeRightEdge = rightEdge;
  }

  // Orphan assets are grouped away from connected assets and folders.
  if (orphanAssets.length > 0) {
    const groupX = treeNodes.length > 0 ? treeRightEdge + STATIC_COLUMN_GAP : 0;
    result.push(
      ...makeNodeGroup(orphanAssets, connectedIds, selectedNodeId, groupX, '无依赖资产', '这些资产没有任何依赖边。'),
    );
  }

  return result;
}

type TreeComparator = (a: BPQAGraphNode, b: BPQAGraphNode) => number;

// Compute tidy tree coordinates: level is depth, order is sibling placement.
function computeTreeLevels(
  treeNodes: BPQAGraphNode[],
  edges: BPQAGraphEdge[],
  comparator: TreeComparator,
): { level: Map<string, number>; order: Map<string, number>; maxLevel: number } {
  const idSet = new Set(treeNodes.map((node) => node.id));
  const children = new Map<string, string[]>();
  const indegree = new Map<string, number>();
  treeNodes.forEach((node) => indegree.set(node.id, 0));

  const validEdge = (edge: BPQAGraphEdge): boolean =>
    idSet.has(edge.from) && idSet.has(edge.to) && edge.from !== edge.to;

  edges.forEach((edge) => {
    if (!validEdge(edge)) {
      return;
    }
    const list = children.get(edge.from) ?? [];
    list.push(edge.to);
    children.set(edge.from, list);
    indegree.set(edge.to, (indegree.get(edge.to) ?? 0) + 1);
  });

  const level = new Map<string, number>();
  treeNodes.forEach((node) => level.set(node.id, 0));
  for (let iteration = 0; iteration < treeNodes.length + 1; iteration += 1) {
    let changed = false;
    edges.forEach((edge) => {
      if (!validEdge(edge)) {
        return;
      }
      const candidate = (level.get(edge.from) ?? 0) + 1;
      if (candidate > (level.get(edge.to) ?? 0)) {
        level.set(edge.to, candidate);
        changed = true;
      }
    });
    if (!changed) {
      break;
    }
  }

  const nodeById = new Map(treeNodes.map((node) => [node.id, node] as const));
  const order = new Map<string, number>();
  const visited = new Set<string>();
  let nextLeaf = 0;

  const sortedChildren = (id: string): string[] =>
    [...(children.get(id) ?? [])]
      .filter((childId) => idSet.has(childId))
      .sort((a, b) => comparator(nodeById.get(a)!, nodeById.get(b)!));

  const assign = (id: string): number => {
    if (visited.has(id)) {
      return order.get(id) ?? 0;
    }
    visited.add(id);

    const childOrders: number[] = [];
    sortedChildren(id).forEach((childId) => {
      childOrders.push(visited.has(childId) ? order.get(childId) ?? 0 : assign(childId));
    });

    const value = childOrders.length === 0 ? nextLeaf++ : (Math.min(...childOrders) + Math.max(...childOrders)) / 2;
    order.set(id, value);
    return value;
  };

  const roots = treeNodes.filter((node) => (indegree.get(node.id) ?? 0) === 0).sort(comparator);
  (roots.length > 0 ? roots : [...treeNodes].sort(comparator)).forEach((node) => assign(node.id));
  // Add unvisited nodes as leaves so isolated folders still render.
  [...treeNodes].sort(comparator).forEach((node) => {
    if (!visited.has(node.id)) {
      order.set(node.id, nextLeaf++);
      visited.add(node.id);
    }
  });

  let maxLevel = 0;
  treeNodes.forEach((node) => {
    maxLevel = Math.max(maxLevel, level.get(node.id) ?? 0);
  });

  return { level, order, maxLevel };
}

// Static/folder dependencies use a tidy tree from left to right.
function layoutTree(
  treeNodes: BPQAGraphNode[],
  edges: BPQAGraphEdge[],
  connectedIds: Set<string>,
  selectedNodeId: string,
  mode: BPQAViewMode,
  comparator: TreeComparator,
): { laidOut: BPQAAnyNode[]; rightEdge: number } {
  const { level, order, maxLevel } = hasMeaningfulExplicitDepth(treeNodes)
    ? computeExplicitTreeLevels(treeNodes, comparator)
    : computeTreeLevels(treeNodes, edges, comparator);
  const laidOut = treeNodes.map((node) =>
    makeFlowNode(node, connectedIds, selectedNodeId, false, mode, {
      x: (order.get(node.id) ?? 0) * STATIC_COLUMN_GAP,
      y: (level.get(node.id) ?? 0) * STATIC_ROW_GAP,
    }),
  );

  const rightEdge = Math.max(...laidOut.map((node) => node.position.x + readAnyNodeWidth(node)), NODE_WIDTH);
  return { laidOut, rightEdge };
}

// Runtime flow uses a top-down tree ordered by observed execution/loading sequence.
function layoutRuntimeNodes(
  nodes: BPQAGraphNode[],
  edges: BPQAGraphEdge[],
  connectedIds: Set<string>,
  selectedNodeId: string,
): BPQAAnyNode[] {
  if (nodes.length === 0) {
    return [];
  }

  const byLoadOrder: TreeComparator = (a, b) => (a.timeSeconds ?? 0) - (b.timeSeconds ?? 0) || compareNodes(a, b);
  if (hasMeaningfulExplicitDepth(nodes)) {
    return layoutRuntimeReferenceTree(nodes, connectedIds, selectedNodeId, byLoadOrder);
  }

  const { level, order } = computeTreeLevels(nodes, edges, byLoadOrder);
  const levelCounts = new Map<number, number>();
  return nodes.map((node) =>
  {
    const nodeLevel = level.get(node.id) ?? 0;
    const levelIndex = levelCounts.get(nodeLevel) ?? 0;
    levelCounts.set(nodeLevel, levelIndex + 1);
    return makeFlowNode(node, connectedIds, selectedNodeId, true, 'RuntimeFlow', {
      x: (order.get(node.id) ?? 0) * RUNTIME_NODE_COLUMN_GAP,
      y: nodeLevel * RUNTIME_NODE_ROW_GAP + getRuntimeStaggerY(levelIndex),
    });
  });
}

function layoutRuntimeReferenceTree(
  nodes: BPQAGraphNode[],
  connectedIds: Set<string>,
  selectedNodeId: string,
  comparator: TreeComparator,
): BPQAAnyNode[] {
  const groups = new Map<number, BPQAGraphNode[]>();
  nodes.forEach((node) => {
    const depth = Math.max(0, Math.min(3, Math.trunc(node.depth ?? 0)));
    const list = groups.get(depth) ?? [];
    list.push(node);
    groups.set(depth, list);
  });

  const laneLayouts = new Map<number, { columns: number; width: number; height: number }>();
  RUNTIME_LANES.forEach((lane) => {
    const nodeCount = groups.get(lane.depth)?.length ?? 0;
    const columns = lane.depth === 3 ? 1 : Math.max(1, Math.min(5, Math.ceil(Math.sqrt(Math.max(1, nodeCount)) * 1.35)));
    const rows = Math.max(1, Math.ceil(Math.max(1, nodeCount) / columns));
    laneLayouts.set(lane.depth, {
      columns,
      width: Math.max(lane.width, 72 + columns * RUNTIME_NODE_COLUMN_GAP),
      height: Math.max(lane.height, 118 + rows * RUNTIME_NODE_ROW_GAP + RUNTIME_NODE_STAGGER_Y),
    });
  });

  const result: BPQAAnyNode[] = RUNTIME_LANES.map((lane) => {
    const layout = laneLayouts.get(lane.depth) ?? { columns: lane.columns, width: lane.width, height: lane.height };
    return {
      id: lane.id,
      type: 'lane',
      position: { x: lane.x, y: lane.y },
      data: { title: lane.title },
      draggable: false,
      selectable: false,
      zIndex: 0,
      style: { width: layout.width, height: layout.height },
      origin: NODE_ORIGIN,
    };
  });

  RUNTIME_LANES.forEach((lane) => {
    const layout = laneLayouts.get(lane.depth) ?? { columns: lane.columns, width: lane.width, height: lane.height };
    const laneNodes = [...(groups.get(lane.depth) ?? [])].sort(comparator);
    laneNodes.forEach((node, index) => {
      const column = index % layout.columns;
      const row = Math.floor(index / layout.columns);
      result.push({
        ...makeFlowNode(node, connectedIds, selectedNodeId, true, 'RuntimeFlow', {
          x: 40 + column * RUNTIME_NODE_COLUMN_GAP,
          y: 78 + row * RUNTIME_NODE_ROW_GAP + getRuntimeStaggerY(column),
        }),
        parentId: lane.id,
        extent: 'parent',
      });
    });
  });

  return result;
}

function makeNodeGroup(
  groupNodes: BPQAGraphNode[],
  connectedIds: Set<string>,
  selectedNodeId: string,
  x: number,
  title: string,
  detail: string,
): BPQAAnyNode[] {
  const columns = Math.min(3, Math.max(1, Math.ceil(Math.sqrt(groupNodes.length))));
  const rows = Math.ceil(groupNodes.length / columns);
  const width = columns * INACTIVE_COLUMN_GAP + 34;
  const height = rows * INACTIVE_ROW_GAP + 110;
  const groupId = '__inactive_group';
  const result: BPQAAnyNode[] = [
    {
      id: groupId,
      type: 'annotation',
      position: { x, y: -34 },
      data: {
        title,
        detail,
      },
      draggable: false,
      selectable: false,
      zIndex: 0,
      style: { width, height },
      origin: NODE_ORIGIN,
    },
  ];

  [...groupNodes].sort(compareNodes).forEach((node, index) => {
    const column = index % columns;
    const row = Math.floor(index / columns);
    result.push({
      ...makeFlowNode(node, connectedIds, selectedNodeId, false, 'StaticDependency', {
        x: 18 + column * INACTIVE_COLUMN_GAP,
        y: 72 + row * INACTIVE_ROW_GAP,
      }),
      parentId: groupId,
      extent: 'parent',
    });
  });

  return result;
}

function isStaticCollapseMode(mode: BPQAViewMode): boolean {
  return mode === 'StaticDependency' || mode === 'FolderDependency' || mode === 'FolderAssetDependency';
}

function resolveNodeCollisions(nodes: BPQAAnyNode[]): BPQAAnyNode[] {
  const result = [...nodes];
  const groups = new Map<string, BPQAFlowNode[]>();

  result.forEach((node) => {
    if (node.type !== 'bpqaNode' || node.hidden) {
      return;
    }
    const groupKey = `${node.parentId ?? '__root'}:${Math.round(node.position.y / 4) * 4}`;
    const group = groups.get(groupKey) ?? [];
    group.push(node);
    groups.set(groupKey, group);
  });

  const positionOverrides = new Map<string, { x: number; y: number }>();
  groups.forEach((group) => {
    let nextX = Number.NEGATIVE_INFINITY;
    [...group].sort((a, b) => a.position.x - b.position.x || a.id.localeCompare(b.id)).forEach((node) => {
      const width = readNodeWidth(node);
      const x = Math.max(node.position.x, nextX);
      positionOverrides.set(node.id, { x, y: node.position.y });
      nextX = x + width + COLLISION_NODE_GAP;
    });
  });

  return result.map((node) => {
    const nextPosition = positionOverrides.get(node.id);
    return nextPosition ? { ...node, position: nextPosition } : node;
  });
}

function applyExecutionOrder(nodes: BPQAAnyNode[], mode: BPQAViewMode): BPQAAnyNode[] {
  if (mode !== 'RuntimeFlow' && mode !== 'NodeFlow') {
    return nodes;
  }

  const orderById = new Map<string, number>();
  nodes
    .filter((node): node is BPQAFlowNode => node.type === 'bpqaNode' && !node.hidden)
    .sort(compareFlowNodesByExecutionOrder)
    .forEach((node, index) => orderById.set(node.id, index + 1));

  return nodes.map((node) => {
    if (node.type !== 'bpqaNode') {
      return node;
    }

    const executionOrder = orderById.get(node.id);
    return executionOrder
      ? {
          ...node,
          data: {
            ...node.data,
            executionOrder,
          },
        }
      : node;
  });
}

function compareFlowNodesByExecutionOrder(a: BPQAFlowNode, b: BPQAFlowNode): number {
  const aTime = a.data.bpqa?.timeSeconds;
  const bTime = b.data.bpqa?.timeSeconds;
  const aHasTime = Number.isFinite(aTime);
  const bHasTime = Number.isFinite(bTime);
  if (aHasTime && bHasTime && aTime !== bTime) {
    return (aTime ?? 0) - (bTime ?? 0);
  }
  if (aHasTime !== bHasTime) {
    return aHasTime ? -1 : 1;
  }

  return (
    a.position.y - b.position.y ||
    a.position.x - b.position.x ||
    a.id.localeCompare(b.id, undefined, { numeric: true, sensitivity: 'base' })
  );
}

function collapseLargeStaticTree(
  nodes: BPQAAnyNode[],
  edges: BPQAFlowEdge[],
): { nodes: BPQAAnyNode[]; edges: BPQAFlowEdge[] } {
  const graphNodes = nodes.filter((node): node is BPQAFlowNode => node.type === 'bpqaNode');
  if (graphNodes.length <= STATIC_TREE_COLLAPSE_NODE_LIMIT) {
    return { nodes, edges };
  }

  const sortedGraphNodes = [...graphNodes].sort(compareFlowNodesByPosition);
  const visibleIds = new Set(sortedGraphNodes.slice(0, STATIC_TREE_COLLAPSE_NODE_LIMIT).map((node) => node.id));
  const foldedIds = new Set(sortedGraphNodes.slice(STATIC_TREE_COLLAPSE_NODE_LIMIT).map((node) => node.id));
  const nodesWithFoldState = nodes.map((node) => {
    if (node.type === 'bpqaNode' && foldedIds.has(node.id)) {
      return { ...node, hidden: true, selected: false };
    }

    return node;
  });
  const edgesWithFoldState = edges.map((edge) =>
    foldedIds.has(edge.source) || foldedIds.has(edge.target) ? { ...edge, hidden: true } : edge,
  );
  const visibleBounds = getNodeBounds(nodesWithFoldState.filter((node) => !node.hidden));

  return {
    nodes: [
      ...nodesWithFoldState,
      {
        id: '__static_tree_fold',
        type: 'annotation',
        position: { x: visibleBounds.minX, y: visibleBounds.maxY + 72 },
        data: {
          title: '已折叠大型节点树',
          detail: `隐藏 ${foldedIds.size} 个节点和相关依赖边，取消勾选可展开。`,
        },
        draggable: false,
        selectable: false,
        zIndex: 1,
        style: { width: 286, height: 104 },
        origin: NODE_ORIGIN,
      },
    ],
    edges: edgesWithFoldState,
  };
}

function compareFlowNodesByPosition(a: BPQAFlowNode, b: BPQAFlowNode): number {
  return (
    (a.parentId ?? '').localeCompare(b.parentId ?? '') ||
    a.position.x - b.position.x ||
    a.position.y - b.position.y ||
    a.id.localeCompare(b.id)
  );
}

function readNodeWidth(node: BPQAFlowNode): number {
  const width = node.style?.width;
  return typeof width === 'number' ? width : NODE_WIDTH;
}

function readAnyNodeWidth(node: BPQAAnyNode): number {
  const width = node.style?.width;
  return typeof width === 'number' ? width : NODE_WIDTH;
}

function readAnyNodeHeight(node: BPQAAnyNode): number {
  const height = node.style?.height ?? node.style?.minHeight;
  return typeof height === 'number' ? height : NODE_HEIGHT;
}

function makeAbsoluteNodePositionMap(nodes: BPQAAnyNode[]): Map<string, { x: number; y: number }> {
  const nodesById = new Map(nodes.map((node) => [node.id, node] as const));
  const cache = new Map<string, { x: number; y: number }>();

  const resolve = (node: BPQAAnyNode): { x: number; y: number } => {
    const cached = cache.get(node.id);
    if (cached) {
      return cached;
    }

    const parent = node.parentId ? nodesById.get(node.parentId) : undefined;
    const parentPosition = parent ? resolve(parent) : { x: 0, y: 0 };
    const position = {
      x: parentPosition.x + node.position.x,
      y: parentPosition.y + node.position.y,
    };
    cache.set(node.id, position);
    return position;
  };

  nodes.forEach((node) => resolve(node));
  return cache;
}

function makeDistributedRouteOffset(index: number, count: number, mode: BPQAViewMode): number {
  if (count <= 1) {
    return 0;
  }

  const magnitude = Math.floor(index / 2) + 1;
  const direction = index % 2 === 0 ? -1 : 1;
  if (mode === 'RuntimeFlow') {
    return direction * (EDGE_SIDE_ROUTE_BASE_OFFSET + (magnitude - 1) * EDGE_ROUTE_OFFSET_GAP);
  }

  return direction * magnitude * EDGE_ROUTE_OFFSET_GAP;
}

function shouldUseSideRoute(
  sourceNode: BPQAAnyNode | undefined,
  targetNode: BPQAAnyNode | undefined,
  sourcePosition: { x: number; y: number },
  targetPosition: { x: number; y: number },
): boolean {
  if (!sourceNode || !targetNode || sourceNode.id === targetNode.id) {
    return false;
  }

  const sameLayerThreshold = Math.max(12, RUNTIME_NODE_STAGGER_Y + 4, COLLISION_NODE_GAP / 2);
  const sameParent = (sourceNode.parentId ?? '__root') === (targetNode.parentId ?? '__root');
  const sameVisualRow = sameParent && Math.abs(sourcePosition.y - targetPosition.y) <= sameLayerThreshold;
  const backEdge = sourcePosition.y > targetPosition.y + sameLayerThreshold;
  return sameVisualRow || backEdge;
}

function makeSingleEdgeSideRouteOffset(sourceX: number, targetX: number): number {
  return (sourceX <= targetX ? 1 : -1) * EDGE_SIDE_ROUTE_BASE_OFFSET;
}

function getRuntimeStaggerY(index: number): number {
  return index % 2 === 0 ? 0 : RUNTIME_NODE_STAGGER_Y;
}

function getNodeBounds(nodes: BPQAAnyNode[]): { minX: number; minY: number; maxX: number; maxY: number } {
  if (nodes.length === 0) {
    return { minX: 0, minY: 0, maxX: NODE_WIDTH, maxY: NODE_HEIGHT };
  }

  let minX = Number.POSITIVE_INFINITY;
  let minY = Number.POSITIVE_INFINITY;
  let maxX = Number.NEGATIVE_INFINITY;
  let maxY = Number.NEGATIVE_INFINITY;

  nodes.forEach((node) => {
    minX = Math.min(minX, node.position.x);
    minY = Math.min(minY, node.position.y);
    maxX = Math.max(maxX, node.position.x + readAnyNodeWidth(node));
    maxY = Math.max(maxY, node.position.y + readAnyNodeHeight(node));
  });

  return { minX, minY, maxX, maxY };
}

function hasMeaningfulExplicitDepth(nodes: BPQAGraphNode[]): boolean {
  return nodes.some((node) => Number.isFinite(node.depth) && (node.depth ?? 0) > 0);
}

function computeExplicitTreeLevels(
  treeNodes: BPQAGraphNode[],
  comparator: TreeComparator,
): { level: Map<string, number>; order: Map<string, number>; maxLevel: number } {
  const level = new Map<string, number>();
  const order = new Map<string, number>();
  const grouped = new Map<number, BPQAGraphNode[]>();
  let maxLevel = 0;

  treeNodes.forEach((node) => {
    const explicitLevel = Math.max(0, Math.trunc(node.depth ?? 0));
    level.set(node.id, explicitLevel);
    maxLevel = Math.max(maxLevel, explicitLevel);
    const list = grouped.get(explicitLevel) ?? [];
    list.push(node);
    grouped.set(explicitLevel, list);
  });

  grouped.forEach((nodesAtLevel) => {
    [...nodesAtLevel].sort(comparator).forEach((node, index) => {
      order.set(node.id, index);
    });
  });

  return { level, order, maxLevel };
}

function makeFlowNode(
  node: BPQAGraphNode,
  connectedIds: Set<string>,
  selectedNodeId: string,
  compact: boolean,
  mode: BPQAViewMode,
  position: { x: number; y: number },
): BPQAFlowNode {
  const connected = connectedIds.has(node.id);
  const title = truncateText(node.label || node.id, compact ? 24 : 28);
  const subtitle = truncateText(node.type || node.className || 'Asset', compact ? 26 : 30);
  const detail = truncateText(node.assetPath || node.packageName || node.folderPath || node.tooltip || node.id, compact ? 34 : 42);

  return {
    id: node.id,
    type: 'bpqaNode',
    position,
    data: {
      bpqa: node,
      title,
      subtitle,
      detail,
      visual: getNodeVisual(node, connected, mode),
      connected,
      compact,
      viewMode: mode,
    },
    selected: selectedNodeId === node.id,
    sourcePosition: Position.Bottom,
    targetPosition: Position.Top,
    draggable: true,
    zIndex: 2,
    style: { width: compact ? 218 : NODE_WIDTH, minHeight: compact ? 78 : NODE_HEIGHT },
    origin: NODE_ORIGIN,
  };
}

function compareNodesByTimeline(a: BPQAGraphNode, b: BPQAGraphNode): number {
  return (a.timeSeconds ?? 0) - (b.timeSeconds ?? 0) || compareNodes(a, b);
}


function compareNodes(a: BPQAGraphNode, b: BPQAGraphNode): number {
  return (a.label || a.id).localeCompare(b.label || b.id, undefined, { numeric: true, sensitivity: 'base' });
}

function stableDomId(value: string): string {
  return value.replace(/[^a-zA-Z0-9_-]/g, '_').slice(0, 80) || 'node';
}
