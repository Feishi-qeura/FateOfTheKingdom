import type { Edge, Node } from '@xyflow/react';

export type BPQAViewMode =
  | 'StaticDependency'
  | 'FolderDependency'
  | 'FolderAssetDependency'
  | 'RuntimeFlow'
  | 'NodeFlow'
  | 'Unknown';

export interface BPQAGraphNode {
  id: string;
  label?: string;
  type?: string;
  assetPath?: string;
  className?: string;
  parentClass?: string;
  packageName?: string;
  folderPath?: string;
  tooltip?: string;
  timeSeconds?: number;
  depth?: number;
  isBlueprint?: boolean;
  references?: string[];
  referencedBy?: string[];
  interfaces?: string[];
  components?: string[];
}

export interface BPQAGraphEdge {
  from: string;
  to: string;
  reason?: string;
  type?: string;
  timeSeconds?: number;
  shape?: string;
  motionShape?: string;
  duration?: number;
  motionDuration?: number;
}

export interface BPQAGraphPayload {
  title?: string;
  mode?: BPQAViewMode | string;
  nodes?: BPQAGraphNode[];
  edges?: BPQAGraphEdge[];
}

export type BPQANodeVisualVariant =
  | 'inactive'
  | 'blueprint'
  | 'asset'
  | 'folder'
  | 'event'
  | 'function'
  | 'macro'
  | 'construction';

export interface BPQANodeVisual {
  variant: BPQANodeVisualVariant;
  accent: string;
  glow: string;
  background: string;
  foreground: string;
  border: string;
}

export interface BPQAFlowNodeData extends Record<string, unknown> {
  bpqa?: BPQAGraphNode;
  title: string;
  subtitle: string;
  detail: string;
  visual: BPQANodeVisual;
  connected: boolean;
  compact: boolean;
  viewMode: BPQAViewMode;
  executionOrder?: number;
}

export interface BPQAAnnotationData extends Record<string, unknown> {
  title: string;
  detail: string;
}

export interface BPQALaneData extends Record<string, unknown> {
  title: string;
}

export interface BPQAFlowEdgeData extends Record<string, unknown> {
  bpqa: BPQAGraphEdge;
  accent: string;
  glow: string;
  label: string;
  viewMode: BPQAViewMode;
  routeOffset?: number;
  routeLane?: number;
  motionColor?: string;
  motionSpeedSeconds?: number;
  motionFrequency?: number;
  motionTrackVisible?: boolean;
  motionTrackColor?: string;
  shape?: string;
  motionShape?: string;
  duration?: number;
  /** Pre-computed obstacle-avoiding orthogonal path (overrides built-in routing). */
  routePath?: string;
  routeLabelX?: number;
  routeLabelY?: number;
  /** SVG path "d" for a user-supplied custom motion shape (shape === 'custom'). */
  customShapePath?: string;
}

export type BPQAFlowNode = Node<BPQAFlowNodeData, 'bpqaNode'>;
export type BPQAAnnotationNode = Node<BPQAAnnotationData, 'annotation'>;
export type BPQALaneNode = Node<BPQALaneData, 'lane'>;
export type BPQAAnyNode = BPQAFlowNode | BPQAAnnotationNode | BPQALaneNode;
export type BPQAFlowEdge = Edge<BPQAFlowEdgeData, 'step'>;

export interface BPQAMappedGraph {
  title: string;
  mode: BPQAViewMode;
  nodes: BPQAAnyNode[];
  edges: BPQAFlowEdge[];
  sourceNodeCount: number;
  sourceEdgeCount: number;
}
