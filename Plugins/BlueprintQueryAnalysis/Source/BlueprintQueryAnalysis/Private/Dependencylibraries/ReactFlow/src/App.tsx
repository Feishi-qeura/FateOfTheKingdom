import { useCallback, useEffect, useMemo, useRef, useState, type ChangeEvent } from 'react';
import {
  applyNodeChanges,
  Background,
  BackgroundVariant,
  Controls,
  MiniMap,
  Panel,
  ReactFlow,
  ReactFlowProvider,
  useReactFlow,
  useViewport,
  ViewportPortal,
  type Node as ReactFlowNode,
  type NodeChange,
  type NodeMouseHandler,
  type NodeProps,
  type OnMoveEnd,
  type OnNodeDrag,
  type SnapGrid,
  type Viewport,
} from '@xyflow/react';
import { BPQAEdge } from './BPQAEdge';
import { BPQANode } from './BPQANode';
import { createHostBridge, notifyHostOpen, notifyHostRendered, notifyHostSelect, sendImageToHost } from './hostBridge';
import { mapGraphToReactFlow } from './graphMapper';
import { computeLayout, isElkMode, isForceMode, type PositionMap } from './layoutEngine';
import { computeOrthogonalRoutes } from './edgeRouting';
import type {
  BPQAAnnotationData,
  BPQAAnnotationNode,
  BPQAAnyNode,
  BPQAFlowEdge,
  BPQAFlowNode,
  BPQAGraphPayload,
  BPQAMappedGraph,
  BPQANodeVisual,
  BPQALaneData,
  BPQALaneNode,
  BPQAViewMode,
} from './types';

type ViewState =
  | { kind: 'idle' }
  | { kind: 'loading'; message: string; progress: number }
  | { kind: 'error'; message: string };

type HelperLines = { x?: number; y?: number };

const EMPTY_STATIC_GRAPH: BPQAGraphPayload = {
  title: 'Blueprint Insight',
  mode: 'StaticDependency',
  nodes: [],
  edges: [],
};

const MIN_ZOOM = 0.12;
const MAX_ZOOM = 2.2;
const FALLBACK_NODE_WIDTH = 236;
const FALLBACK_NODE_HEIGHT = 94;
const DEFAULT_GRID_GAP = 10;
const MIN_GRID_GAP = 4;
const MAX_GRID_GAP = 80;
const FIT_VIEW_OPTIONS = { padding: 0.18 };
const FIT_VIEW_REQUEST = { padding: 0.18, duration: 260, includeHiddenNodes: false };
const PRO_OPTIONS = { hideAttribution: true };
const MINI_MAP_STYLE = {
  background: 'rgba(9, 13, 20, 0.9)',
  border: '1px solid rgba(132, 151, 178, 0.28)',
  borderRadius: 6,
};
const EMPTY_ROUTES: Record<string, { path: string; labelX: number; labelY: number }> = {};
const MOTION_SHAPE_OPTIONS = ['orb', 'box', 'diamond', 'triangle', 'star', 'package', 'custom'] as const;

const nodeTypes = {
  bpqaNode: BPQANode,
  annotation: AnnotationNode,
  lane: LaneNode,
};

const edgeTypes = {
  step: BPQAEdge,
};

export function BlueprintQueryAnalysisApp() {
  return (
    <ReactFlowProvider>
      <BlueprintQueryAnalysisCanvas />
    </ReactFlowProvider>
  );
}

function BlueprintQueryAnalysisCanvas() {
  const reactFlow = useReactFlow();
  const viewport = useViewport();
  const [graph, setGraph] = useState<BPQAGraphPayload>(EMPTY_STATIC_GRAPH);
  const [selectedNodeId, setSelectedNodeId] = useState('');
  const [viewState, setViewState] = useState<ViewState>({ kind: 'idle' });
  const [graphVersion, setGraphVersion] = useState(0);
  const [gridGap, setGridGap] = useState(DEFAULT_GRID_GAP);
  const [collapseLargeStaticTree, setCollapseLargeStaticTree] = useState(true);
  const [flowNodes, setFlowNodes] = useState<BPQAAnyNode[]>([]);
  const [nodePositionOverrides, setNodePositionOverrides] = useState<Record<string, { x: number; y: number }>>({});
  const [helperLines, setHelperLines] = useState<HelperLines>({});
  const [layout, setLayout] = useState<{ signature: string; positions: PositionMap; flatten: boolean } | null>(null);
  const [refreshToken, setRefreshToken] = useState(0);
  const [orthogonalRouting, setOrthogonalRouting] = useState(true);
  const [runtimeEdgeMotionShape, setRuntimeEdgeMotionShape] = useState('orb');
  const [runtimeEdgeCustomShapePath, setRuntimeEdgeCustomShapePath] = useState('');
  const [runtimeEventColor, setRuntimeEventColor] = useState('#ff5c6f');
  const [runtimeFunctionColor, setRuntimeFunctionColor] = useState('#5ea7ff');
  const [runtimeEdgeMotionTrackVisible, setRuntimeEdgeMotionTrackVisible] = useState(true);
  const [runtimeEdgeMotionTrackColor, setRuntimeEdgeMotionTrackColor] = useState('#5ea7ff');
  const [runtimeEdgeMotionColor, setRuntimeEdgeMotionColor] = useState('#ff2f8f');
  const [runtimeEdgeMotionSpeed, setRuntimeEdgeMotionSpeed] = useState(1.15);
  const [runtimeEdgeMotionFrequency, setRuntimeEdgeMotionFrequency] = useState(3);
  const [appBackgroundColor, setAppBackgroundColor] = useState('#070a10');
  const renderSerialRef = useRef(0);
  const lastFitSignatureRef = useRef('');
  const lastRenderedSignatureRef = useRef('');
  const selectedNodeIdRef = useRef('');
  const lastHostGraphSignatureRef = useRef('');
  const wrapperRef = useRef<HTMLDivElement>(null);
  const mappedGraph = useMemo(
    () => mapGraphToReactFlow(graph, '', { collapseLargeStaticTree }),
    [collapseLargeStaticTree, graph],
  );
  const snapGrid = useMemo<SnapGrid>(() => [gridGap, gridGap], [gridGap]);
  const graphContentSignature = useMemo(() => makeGraphContentSignature(graph), [graph]);
  const fitContentSignature = useMemo(
    () => `${graphContentSignature}:${collapseLargeStaticTree ? 'folded' : 'expanded'}`,
    [collapseLargeStaticTree, graphContentSignature],
  );
  // Layout identity: changes when the graph content changes, or when the user
  // presses the manual refresh button. Used to gate one-shot layout + fit.
  const layoutSignature = useMemo(() => `${fitContentSignature}:${refreshToken}`, [fitContentSignature, refreshToken]);

  // Render straight from the mapped graph; the old progressive-reveal timer that
  // continuously re-rendered (and disturbed cursor/zoom state) has been removed.
  const presentedGraph = mappedGraph;

  // Nodes with the active external layout (ELK / D3) and user drag overrides
  // applied. Visuals + selection are layered on separately so edge routes only
  // recompute when node positions actually change.
  const positionedBaseNodes = useMemo(
    () =>
      applyPositionOverrides(
        applyExternalLayout(presentedGraph.nodes, layout, layoutSignature),
        nodePositionOverrides,
      ),
    [layout, layoutSignature, nodePositionOverrides, presentedGraph.nodes],
  );

  const edgeRoutes = useMemo(
    () => (orthogonalRouting ? computeOrthogonalRoutes(positionedBaseNodes, presentedGraph.edges) : EMPTY_ROUTES),
    [orthogonalRouting, positionedBaseNodes, presentedGraph.edges],
  );

  const presentedEdges = useMemo(
    () =>
      applyEdgeRouteSettings(
        applyEdgeMotionSettings(
          presentedGraph.edges,
          presentedGraph.mode,
          runtimeEdgeMotionColor,
          runtimeEdgeMotionSpeed,
          runtimeEdgeMotionFrequency,
          runtimeEdgeMotionTrackVisible,
          runtimeEdgeMotionTrackColor,
          runtimeEdgeMotionShape,
          runtimeEdgeCustomShapePath,
        ),
        edgeRoutes,
      ),
    [
      edgeRoutes,
      presentedGraph.edges,
      presentedGraph.mode,
      runtimeEdgeCustomShapePath,
      runtimeEdgeMotionColor,
      runtimeEdgeMotionFrequency,
      runtimeEdgeMotionShape,
      runtimeEdgeMotionSpeed,
      runtimeEdgeMotionTrackColor,
      runtimeEdgeMotionTrackVisible,
    ],
  );
  const isContextualZoomCompressed = viewport.zoom < 0.42;

  useEffect(() => {
    selectedNodeIdRef.current = selectedNodeId;
  }, [selectedNodeId]);

  useEffect(() => {
    setFlowNodes(
      applyNodeSelection(
        applyNodeVisualSettings(
          positionedBaseNodes,
          mappedGraph.mode,
          runtimeEventColor,
          runtimeFunctionColor,
        ),
        selectedNodeId,
      ),
    );
  }, [mappedGraph.mode, positionedBaseNodes, runtimeEventColor, runtimeFunctionColor, selectedNodeId]);

  // Run the external layout engine once per graph change / manual refresh.
  // ELK (runtime + node flow) and D3 force (static dependency) compute final
  // positions asynchronously; results are gated by layoutSignature so stale
  // positions from a previous graph are never applied.
  useEffect(() => {
    let active = true;
    void (async () => {
      const result = await computeLayout(mappedGraph.mode, mappedGraph.nodes, mappedGraph.edges);
      if (!active) {
        return;
      }
      if (result.engine === 'none') {
        setLayout(null);
        return;
      }
      setLayout({ signature: layoutSignature, positions: result.positions, flatten: result.flatten });
    })();
    return () => {
      active = false;
    };
  }, [layoutSignature, mappedGraph]);

  useEffect(() => {
    return createHostBridge({
      onGraph: (nextGraph) => {
        const safeGraph = nextGraph ?? EMPTY_STATIC_GRAPH;
        const nextSignature = makeGraphContentSignature(safeGraph);
        lastHostGraphSignatureRef.current = nextSignature;
        setGraph(safeGraph);
        setNodePositionOverrides({});
        setViewState({ kind: 'idle' });
        setGraphVersion((version) => version + 1);
      },
      onSelected: (nodeId) => setSelectedNodeId(nodeId),
      onLoading: (message, progress) =>
        setViewState(message ? { kind: 'loading', message, progress } : { kind: 'idle' }),
    });
  }, []);

  useEffect(() => {
    // Fit exactly once per graph change / refresh. For modes with an async
    // external layout we wait until that layout has settled so the view does not
    // snap from the fallback positions to the final ones. Repeated host updates
    // with identical content never reset the viewport.
    const usesExternalLayout = isElkMode(mappedGraph.mode) || isForceMode(mappedGraph.mode);
    const layoutReady = layout != null && layout.signature === layoutSignature;
    const shouldFit =
      layoutSignature !== lastFitSignatureRef.current &&
      (!usesExternalLayout || layoutReady) &&
      mappedGraph.nodes.length > 0;
    const serial = ++renderSerialRef.current;
    const frame = window.requestAnimationFrame(() => {
      if (shouldFit) {
        lastFitSignatureRef.current = layoutSignature;
        reactFlow.fitView(FIT_VIEW_REQUEST);
      }
      window.requestAnimationFrame(() => {
        // Tell the host the graph rendered exactly once per content/refresh, so
        // we don't spam host notifications as the async layout settles.
        if (serial === renderSerialRef.current && lastRenderedSignatureRef.current !== layoutSignature) {
          lastRenderedSignatureRef.current = layoutSignature;
          notifyHostRendered(mappedGraph.sourceNodeCount);
        }
      });
    });

    return () => window.cancelAnimationFrame(frame);
  }, [layout, layoutSignature, mappedGraph.mode, mappedGraph.nodes.length, mappedGraph.sourceNodeCount, reactFlow]);

  // Contextual zoom anchor: selected node first, then the current pointer position.
  useEffect(() => {
    const el = wrapperRef.current;
    if (!el) {
      return;
    }

    const onWheel = (event: WheelEvent) => {
      event.preventDefault();
      const viewport = reactFlow.getViewport();
      const factor = Math.pow(1.0018, -event.deltaY);
      const newZoom = snapZoom(viewport.zoom * factor, gridGap);
      if (newZoom === viewport.zoom) {
        return;
      }

      const selectedNode = selectedNodeIdRef.current ? reactFlow.getNode(selectedNodeIdRef.current) : undefined;
      let anchorX: number;
      let anchorY: number;
      let anchorFlowX: number;
      let anchorFlowY: number;

      if (selectedNode) {
        const width = selectedNode.measured?.width ?? FALLBACK_NODE_WIDTH;
        const height = selectedNode.measured?.height ?? FALLBACK_NODE_HEIGHT;
        anchorFlowX = selectedNode.position.x + width / 2;
        anchorFlowY = selectedNode.position.y + height / 2;
        anchorX = anchorFlowX * viewport.zoom + viewport.x;
        anchorY = anchorFlowY * viewport.zoom + viewport.y;
      } else {
        const rect = el.getBoundingClientRect();
        anchorX = event.clientX - rect.left;
        anchorY = event.clientY - rect.top;
        anchorFlowX = (anchorX - viewport.x) / viewport.zoom;
        anchorFlowY = (anchorY - viewport.y) / viewport.zoom;
      }

      reactFlow.setViewport(snapViewport({
        zoom: newZoom,
        x: anchorX - anchorFlowX * newZoom,
        y: anchorY - anchorFlowY * newZoom,
      }, gridGap));
    };

    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  }, [gridGap, reactFlow]);

  const onNodeClick = useCallback<NodeMouseHandler>((_, node) => {
    if (!isSyntheticNode(node.id)) {
      setSelectedNodeId(node.id);
      notifyHostSelect(node.id);
    }
  }, []);

  const onNodeDoubleClick = useCallback<NodeMouseHandler>((_, node) => {
    if (!isSyntheticNode(node.id)) {
      setSelectedNodeId(node.id);
      notifyHostOpen(node.id);
    }
  }, []);

  const onNodesChange = useCallback((changes: NodeChange<BPQAAnyNode>[]) => {
    setFlowNodes((nodes) => applyNodeChanges(changes, nodes) as BPQAAnyNode[]);
  }, []);

  const onNodeDrag = useCallback<OnNodeDrag<BPQAAnyNode>>(
    (_, node) => {
      setHelperLines(makeHelperLines(node, flowNodes, gridGap));
    },
    [flowNodes, gridGap],
  );

  const onNodeDragStop = useCallback<OnNodeDrag<BPQAAnyNode>>(
    (_, node) => {
      setHelperLines({});
      if (isSyntheticNode(node.id)) {
        return;
      }

      setNodePositionOverrides((overrides) => ({
        ...overrides,
        [node.id]: {
          x: snapValue(node.position.x, gridGap),
          y: snapValue(node.position.y, gridGap),
        },
      }));
    },
    [gridGap],
  );

  const onMoveEnd = useCallback<OnMoveEnd>(
    (_, viewport) => {
      const snappedViewport = snapViewport(viewport, gridGap);
      if (!isSameViewport(viewport, snappedViewport)) {
        reactFlow.setViewport(snappedViewport);
      }
    },
    [gridGap, reactFlow],
  );

  const onGridGapChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setGridGap(clampGridGap(Number(event.currentTarget.value)));
  }, []);

  const onCollapseLargeStaticTreeChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setCollapseLargeStaticTree(event.currentTarget.checked);
  }, []);

  const onRuntimeEdgeMotionColorChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setRuntimeEdgeMotionColor(event.currentTarget.value);
  }, []);

  const onRuntimeEdgeMotionTrackVisibleChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setRuntimeEdgeMotionTrackVisible(event.currentTarget.checked);
  }, []);

  const onRuntimeEdgeMotionTrackColorChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setRuntimeEdgeMotionTrackColor(event.currentTarget.value);
  }, []);

  const onRuntimeEventColorChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setRuntimeEventColor(event.currentTarget.value);
  }, []);

  const onRuntimeFunctionColorChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setRuntimeFunctionColor(event.currentTarget.value);
  }, []);

  const onBackgroundColorChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setAppBackgroundColor(event.currentTarget.value);
  }, []);

  const onRuntimeEdgeMotionSpeedChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setRuntimeEdgeMotionSpeed(clampNumber(Number(event.currentTarget.value), 0.25, 6, 1.15));
  }, []);

  const onRuntimeEdgeMotionFrequencyChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setRuntimeEdgeMotionFrequency(Math.round(clampNumber(Number(event.currentTarget.value), 1, 8, 3)));
  }, []);

  // Manual refresh: re-run the layout engine + fit once, without continuously
  // re-rendering. Dropping accumulated drag overrides gives a clean re-layout.
  const onRefreshView = useCallback(() => {
    setNodePositionOverrides({});
    setRefreshToken((token) => token + 1);
  }, []);

  const onOrthogonalRoutingChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setOrthogonalRouting(event.currentTarget.checked);
  }, []);

  const onRuntimeEdgeMotionShapeChange = useCallback((event: ChangeEvent<HTMLSelectElement>) => {
    setRuntimeEdgeMotionShape(event.currentTarget.value);
  }, []);

  const onRuntimeEdgeCustomShapePathChange = useCallback((event: ChangeEvent<HTMLInputElement>) => {
    setRuntimeEdgeCustomShapePath(event.currentTarget.value);
  }, []);

  const onDownloadImage = useCallback(() => {
    void exportGraphPng(
      reactFlow.getNodes() as BPQAAnyNode[],
      reactFlow.getEdges() as BPQAFlowEdge[],
      appBackgroundColor,
      `${sanitizeFileName(getModeLabel(mappedGraph.mode))}.png`,
    );
  }, [appBackgroundColor, mappedGraph.mode, reactFlow]);

  const overlay = makeOverlay(viewState, mappedGraph.mode, mappedGraph.sourceNodeCount);

  return (
    <div
      className={`bpqa-app ${isContextualZoomCompressed ? 'bpqa-app--contextual-zoom' : ''}`}
      ref={wrapperRef}
      style={{ background: appBackgroundColor }}
    >
      <ReactFlow
        nodes={flowNodes}
        edges={presentedEdges}
        nodeTypes={nodeTypes}
        edgeTypes={edgeTypes}
        nodesDraggable
        nodesConnectable={false}
        elementsSelectable
        minZoom={MIN_ZOOM}
        maxZoom={MAX_ZOOM}
        snapToGrid
        snapGrid={snapGrid}
        zoomOnScroll={false}
        zoomOnDoubleClick={false}
        fitView
        fitViewOptions={FIT_VIEW_OPTIONS}
        onlyRenderVisibleElements
        onNodesChange={onNodesChange}
        onNodeClick={onNodeClick}
        onNodeDoubleClick={onNodeDoubleClick}
        onNodeDrag={onNodeDrag}
        onNodeDragStop={onNodeDragStop}
        onMoveEnd={onMoveEnd}
        proOptions={PRO_OPTIONS}
      >
        <Background id="bpqa-grid-lines" color="#273142" gap={gridGap} lineWidth={1} variant={BackgroundVariant.Lines} />
        <Background
          id="bpqa-grid-major"
          color="#334056"
          gap={gridGap * 5}
          lineWidth={1.2}
          variant={BackgroundVariant.Lines}
        />
        <Panel className="bpqa-panel" position="top-left">
          <div className="bpqa-panel__title">{getModeLabel(mappedGraph.mode)}</div>
          <div className="bpqa-panel__meta">
            {mappedGraph.sourceNodeCount} 节点 / {mappedGraph.sourceEdgeCount} 依赖
          </div>
          <button className="bpqa-panel__button" type="button" onClick={onRefreshView}>
            刷新视图
          </button>
          <label className="bpqa-panel__check">
            <input type="checkbox" checked={orthogonalRouting} onChange={onOrthogonalRoutingChange} />
            <span>正交避让布线</span>
          </label>
          <label className="bpqa-panel__field">
            <span>网格间隔</span>
            <input
              min={MIN_GRID_GAP}
              max={MAX_GRID_GAP}
              step={1}
              type="number"
              value={gridGap}
              onChange={onGridGapChange}
            />
          </label>
          <label className="bpqa-panel__field">
            <span>背景颜色</span>
            <input type="color" value={appBackgroundColor} onChange={onBackgroundColorChange} />
          </label>
          {isStaticPanelMode(mappedGraph.mode) ? (
            <label className="bpqa-panel__check">
              <input type="checkbox" checked={collapseLargeStaticTree} onChange={onCollapseLargeStaticTreeChange} />
              <span>折叠大型节点树</span>
            </label>
          ) : null}
          {isMotionEdgeMode(mappedGraph.mode) ? (
            <>
              <label className="bpqa-panel__check">
                <input
                  type="checkbox"
                  checked={runtimeEdgeMotionTrackVisible}
                  onChange={onRuntimeEdgeMotionTrackVisibleChange}
                />
                <span>传送带可见</span>
              </label>
              <label className="bpqa-panel__field">
                <span>传送带颜色</span>
                <input type="color" value={runtimeEdgeMotionTrackColor} onChange={onRuntimeEdgeMotionTrackColorChange} />
              </label>
              <label className="bpqa-panel__field">
                <span>事件颜色</span>
                <input type="color" value={runtimeEventColor} onChange={onRuntimeEventColorChange} />
              </label>
              <label className="bpqa-panel__field">
                <span>函数颜色</span>
                <input type="color" value={runtimeFunctionColor} onChange={onRuntimeFunctionColorChange} />
              </label>
            </>
          ) : null}
          {isMotionEdgeMode(mappedGraph.mode) ? (
            <>
              <label className="bpqa-panel__field">
                <span>物体发光</span>
                <input type="color" value={runtimeEdgeMotionColor} onChange={onRuntimeEdgeMotionColorChange} />
              </label>
              <label className="bpqa-panel__field">
                <span>传送速度</span>
                <input
                  min={0.25}
                  max={6}
                  step={0.05}
                  type="number"
                  value={runtimeEdgeMotionSpeed}
                  onChange={onRuntimeEdgeMotionSpeedChange}
                />
              </label>
              <label className="bpqa-panel__field">
                <span>物体频率</span>
                <input
                  min={1}
                  max={8}
                  step={1}
                  type="number"
                  value={runtimeEdgeMotionFrequency}
                  onChange={onRuntimeEdgeMotionFrequencyChange}
                />
              </label>
              <label className="bpqa-panel__field">
                <span>物体形状</span>
                <select value={runtimeEdgeMotionShape} onChange={onRuntimeEdgeMotionShapeChange}>
                  {MOTION_SHAPE_OPTIONS.map((shape) => (
                    <option key={shape} value={shape}>
                      {getMotionShapeLabel(shape)}
                    </option>
                  ))}
                </select>
              </label>
              {runtimeEdgeMotionShape === 'custom' ? (
                <label className="bpqa-panel__field">
                  <span>自定义路径</span>
                  <input
                    type="text"
                    placeholder="SVG path d，例如 M0,-6 L6,0 L0,6 L-6,0 Z"
                    value={runtimeEdgeCustomShapePath}
                    onChange={onRuntimeEdgeCustomShapePathChange}
                  />
                </label>
              ) : null}
            </>
          ) : null}
          <button className="bpqa-panel__button" type="button" onClick={onDownloadImage}>
            下载 PNG
          </button>
        </Panel>
        <ViewportPortal>
          <HelperLineOverlay helperLines={helperLines} />
        </ViewportPortal>
        <MiniMap
          className="bpqa-minimap"
          nodeColor={getMiniMapNodeColor}
          nodeStrokeColor={getMiniMapNodeStrokeColor}
          nodeBorderRadius={3}
          maskColor="rgba(7, 10, 16, 0.56)"
          position="bottom-left"
          pannable
          zoomable
          style={MINI_MAP_STYLE}
        />
        <Controls showInteractive={false} position="bottom-right" />
      </ReactFlow>
      {overlay ? <StateOverlay overlay={overlay} /> : null}
    </div>
  );
}

function AnnotationNode(props: NodeProps<BPQAAnnotationNode>) {
  const data = props.data as BPQAAnnotationData;
  return (
    <div className="bpqa-annotation">
      <div className="bpqa-annotation__title">{data.title}</div>
      <div className="bpqa-annotation__detail">{data.detail}</div>
    </div>
  );
}

function LaneNode(props: NodeProps<BPQALaneNode>) {
  const data = props.data as BPQALaneData;
  return (
    <div className="bpqa-lane">
      <span>{data.title}</span>
    </div>
  );
}

function HelperLineOverlay(props: { helperLines: HelperLines }) {
  return (
    <>
      {props.helperLines.x !== undefined ? (
        <div className="bpqa-helper-line bpqa-helper-line--vertical" style={{ transform: `translateX(${props.helperLines.x}px)` }} />
      ) : null}
      {props.helperLines.y !== undefined ? (
        <div
          className="bpqa-helper-line bpqa-helper-line--horizontal"
          style={{ transform: `translateY(${props.helperLines.y}px)` }}
        />
      ) : null}
    </>
  );
}

function applyPositionOverrides(
  nodes: BPQAAnyNode[],
  overrides: Record<string, { x: number; y: number }>,
): BPQAAnyNode[] {
  if (Object.keys(overrides).length === 0) {
    return nodes;
  }

  return nodes.map((node) => {
    const override = overrides[node.id];
    return override ? { ...node, position: override } : node;
  });
}

function applyNodeSelection(nodes: BPQAAnyNode[], selectedNodeId: string): BPQAAnyNode[] {
  return nodes.map((node) => {
    if (node.type !== 'bpqaNode') {
      return node;
    }

    const selected = selectedNodeId === node.id;
    return node.selected === selected ? node : { ...node, selected };
  });
}


function applyEdgeMotionSettings(
  edges: BPQAFlowEdge[],
  mode: BPQAViewMode,
  motionColor: string,
  motionSpeedSeconds: number,
  motionFrequency: number,
  motionTrackVisible: boolean,
  motionTrackColor: string,
  motionShape: string,
  customShapePath: string,
): BPQAFlowEdge[] {
  if (!isMotionEdgeMode(mode)) {
    return edges;
  }

  return edges.map((edge) => ({
    ...edge,
    data: edge.data
      ? {
          ...edge.data,
          motionColor,
          motionSpeedSeconds,
          motionFrequency,
          motionTrackVisible,
          motionTrackColor,
          // A per-edge shape from the host graph still wins; the panel selection
          // is the default applied to every motion edge.
          shape: edge.data.bpqa?.shape ?? edge.data.bpqa?.motionShape ?? motionShape,
          customShapePath,
        }
      : edge.data,
  }));
}

// Injects the pre-computed orthogonal route (path + label anchor) onto each edge.
function applyEdgeRouteSettings(
  edges: BPQAFlowEdge[],
  routes: Record<string, { path: string; labelX: number; labelY: number }>,
): BPQAFlowEdge[] {
  if (routes === EMPTY_ROUTES) {
    // Routing disabled: strip any stale route so edges fall back to step paths.
    return edges.map((edge) =>
      edge.data?.routePath
        ? { ...edge, data: { ...edge.data, routePath: undefined, routeLabelX: undefined, routeLabelY: undefined } }
        : edge,
    );
  }

  return edges.map((edge) => {
    const route = routes[edge.id];
    if (!route || !edge.data) {
      return edge;
    }
    return {
      ...edge,
      data: { ...edge.data, routePath: route.path, routeLabelX: route.labelX, routeLabelY: route.labelY },
    };
  });
}

// Applies an external layout (ELK / D3) to the node set. When a layout is active
// for the current graph the lane / annotation scaffolding is dropped and graph
// nodes are flattened to absolute coordinates owned by the layout engine.
function applyExternalLayout(
  nodes: BPQAAnyNode[],
  layout: { signature: string; positions: PositionMap; flatten: boolean } | null,
  signature: string,
): BPQAAnyNode[] {
  if (!layout || layout.signature !== signature || !layout.flatten) {
    return nodes;
  }

  const result: BPQAAnyNode[] = [];
  for (const node of nodes) {
    if (node.type !== 'bpqaNode') {
      // Lane / annotation containers are layout scaffolding; the engine owns
      // placement now, so they are removed to avoid stale parent offsets.
      continue;
    }
    if (node.hidden) {
      result.push(node);
      continue;
    }
    const position = layout.positions[node.id] ?? node.position;
    const { parentId: _parentId, extent: _extent, ...rest } = node;
    result.push({ ...rest, position });
  }
  return result;
}

function isMotionEdgeMode(mode: BPQAViewMode): boolean {
  return mode === 'RuntimeFlow' || mode === 'NodeFlow';
}

function applyNodeVisualSettings(
  nodes: BPQAAnyNode[],
  mode: BPQAViewMode,
  runtimeEventColor: string,
  runtimeFunctionColor: string,
): BPQAAnyNode[] {
  if (mode !== 'RuntimeFlow' && mode !== 'NodeFlow') {
    return nodes;
  }

  return nodes.map((node) => {
    if (node.type !== 'bpqaNode' || !node.data || !node.data.bpqa) {
      return node;
    }

    const semanticKind = classifyNodeVisualKind(node.data.bpqa.type);
    if (!semanticKind) {
      return node;
    }

    const nextVisual = getSemanticVisualOverride(node.data.visual, semanticKind, runtimeEventColor, runtimeFunctionColor);
    if (nextVisual === node.data.visual) {
      return node;
    }

    return {
      ...node,
      data: {
        ...node.data,
        visual: nextVisual,
      },
    };
  });
}

function makeHelperLines(node: BPQAAnyNode, nodes: BPQAAnyNode[], gridGap: number): HelperLines {
  if (node.type !== 'bpqaNode') {
    return {};
  }

  const threshold = Math.max(4, gridGap);
  let bestX: { value: number; distance: number } | undefined;
  let bestY: { value: number; distance: number } | undefined;
  nodes.forEach((candidate) => {
    if (candidate.id === node.id || candidate.hidden || candidate.type !== 'bpqaNode') {
      return;
    }

    const dx = Math.abs(candidate.position.x - node.position.x);
    const dy = Math.abs(candidate.position.y - node.position.y);
    if (dx <= threshold && (!bestX || dx < bestX.distance)) {
      bestX = { value: candidate.position.x, distance: dx };
    }
    if (dy <= threshold && (!bestY || dy < bestY.distance)) {
      bestY = { value: candidate.position.y, distance: dy };
    }
  });

  return {
    x: bestX?.value,
    y: bestY?.value,
  };
}

const EXPORT_PADDING = 64;
const EXPORT_NODE_WIDTH = 236;
const EXPORT_NODE_HEIGHT = 94;
const EXPORT_MAX_DIMENSION = 4000;

// Renders the current graph to a PNG and hands it to the C++ host to save.
//
// We deliberately rebuild the graph as *pure* SVG (rect / text / path) rather
// than cloning the live DOM into a <foreignObject>: Chromium taints a canvas the
// moment a foreignObject-bearing image is drawn onto it, which made
// canvas.toDataURL() throw and silently broke the old export. Native SVG keeps
// the canvas origin-clean so toDataURL() succeeds, and the bytes are then streamed
// to the host (the editor's offline CEF build cannot trigger a browser download).
async function exportGraphPng(
  nodes: BPQAAnyNode[],
  edges: BPQAFlowEdge[],
  backgroundColor: string,
  filename: string,
): Promise<void> {
  const built = buildGraphSvg(nodes, edges, backgroundColor);
  if (!built) {
    return;
  }

  const { svg, width, height } = built;
  const image = new Image();
  image.src = `data:image/svg+xml;charset=utf-8,${encodeURIComponent(svg)}`;
  await new Promise<void>((resolve, reject) => {
    image.onload = () => resolve();
    image.onerror = () => reject(new Error('Failed to render graph image'));
  });

  const scale = Math.min(2, EXPORT_MAX_DIMENSION / Math.max(width, height, 1));
  const canvas = document.createElement('canvas');
  canvas.width = Math.max(1, Math.ceil(width * scale));
  canvas.height = Math.max(1, Math.ceil(height * scale));
  const context = canvas.getContext('2d');
  if (!context) {
    return;
  }

  context.scale(scale, scale);
  context.drawImage(image, 0, 0);

  const dataUrl = canvas.toDataURL('image/png');
  const base64 = dataUrl.slice(dataUrl.indexOf(',') + 1);
  if (base64) {
    sendImageToHost(filename, base64);
  }
}

interface ExportRect {
  x: number;
  y: number;
  w: number;
  h: number;
}

function exportNodeRect(node: BPQAAnyNode): ExportRect {
  const measuredWidth = node.measured?.width;
  const measuredHeight = node.measured?.height;
  const styleWidth = typeof node.width === 'number' ? node.width : undefined;
  const styleHeight = typeof node.height === 'number' ? node.height : undefined;
  return {
    x: node.position.x,
    y: node.position.y,
    w: measuredWidth ?? styleWidth ?? EXPORT_NODE_WIDTH,
    h: measuredHeight ?? styleHeight ?? EXPORT_NODE_HEIGHT,
  };
}

function buildGraphSvg(
  nodes: BPQAAnyNode[],
  edges: BPQAFlowEdge[],
  backgroundColor: string,
): { svg: string; width: number; height: number } | null {
  const drawn = nodes.filter((node): node is BPQAFlowNode => node.type === 'bpqaNode' && !node.hidden);
  if (drawn.length === 0) {
    return null;
  }

  const rects = new Map<string, ExportRect>();
  let minX = Number.POSITIVE_INFINITY;
  let minY = Number.POSITIVE_INFINITY;
  let maxX = Number.NEGATIVE_INFINITY;
  let maxY = Number.NEGATIVE_INFINITY;
  for (const node of drawn) {
    const rect = exportNodeRect(node);
    rects.set(node.id, rect);
    minX = Math.min(minX, rect.x);
    minY = Math.min(minY, rect.y);
    maxX = Math.max(maxX, rect.x + rect.w);
    maxY = Math.max(maxY, rect.y + rect.h);
  }

  // Routed edges can bow outside the node bounding box; extend the canvas to them.
  for (const edge of edges) {
    const path = edge.data?.routePath;
    if (!path) {
      continue;
    }
    for (const point of parsePathPoints(path)) {
      minX = Math.min(minX, point.x);
      minY = Math.min(minY, point.y);
      maxX = Math.max(maxX, point.x);
      maxY = Math.max(maxY, point.y);
    }
  }

  const width = maxX - minX + EXPORT_PADDING * 2;
  const height = maxY - minY + EXPORT_PADDING * 2;
  const tx = -minX + EXPORT_PADDING;
  const ty = -minY + EXPORT_PADDING;

  const edgeMarkup = edges
    .filter((edge) => !edge.hidden && rects.has(edge.source) && rects.has(edge.target))
    .map((edge) => {
      const source = rects.get(edge.source)!;
      const target = rects.get(edge.target)!;
      const path = edge.data?.routePath ?? exportStepPath(source, target);
      const stroke = edge.data?.motionTrackColor ?? edge.data?.accent ?? '#5b6f87';
      return `<path d="${escapeXml(path)}" fill="none" stroke="${escapeXml(stroke)}" stroke-width="2" stroke-linejoin="round" marker-end="url(#bpqa-export-arrow)" />`;
    })
    .join('');

  const nodeMarkup = drawn.map((node) => exportNodeMarkup(node, rects.get(node.id)!)).join('');

  const svg =
    `<svg xmlns="http://www.w3.org/2000/svg" width="${Math.ceil(width)}" height="${Math.ceil(height)}" ` +
    `viewBox="0 0 ${Math.ceil(width)} ${Math.ceil(height)}" font-family="Segoe UI, system-ui, sans-serif">` +
    `<defs><marker id="bpqa-export-arrow" markerWidth="12" markerHeight="12" refX="9" refY="6" orient="auto" markerUnits="strokeWidth">` +
    `<path d="M1,1 L11,6 L1,11 z" fill="context-stroke" /></marker></defs>` +
    `<rect x="0" y="0" width="${Math.ceil(width)}" height="${Math.ceil(height)}" fill="${escapeXml(backgroundColor)}" />` +
    `<g transform="translate(${tx},${ty})">${edgeMarkup}${nodeMarkup}</g>` +
    `</svg>`;

  return { svg, width, height };
}

function exportNodeMarkup(node: BPQAFlowNode, rect: ExportRect): string {
  const visual = node.data.visual;
  const padX = 13;
  const maxChars = Math.max(6, Math.floor((rect.w - padX * 2) / 7));
  const title = escapeXml(truncateText(node.data.title ?? '', maxChars));
  const subtitle = escapeXml(truncateText(node.data.subtitle ?? '', maxChars));
  const detail = escapeXml(truncateText(node.data.detail ?? '', maxChars + 4));

  const parts: string[] = [];
  parts.push(
    `<rect x="${rect.x}" y="${rect.y}" width="${rect.w}" height="${rect.h}" rx="8" ` +
      `fill="${escapeXml(visual.background)}" stroke="${escapeXml(visual.border ?? visual.accent)}" stroke-width="1.5" />`,
  );
  parts.push(
    `<circle cx="${rect.x + 8}" cy="${rect.y + rect.h / 2}" r="4.5" fill="${escapeXml(visual.accent)}" />`,
  );
  if (title) {
    parts.push(
      `<text x="${rect.x + padX + 8}" y="${rect.y + 26}" fill="${escapeXml(visual.foreground)}" font-size="13" font-weight="700">${title}</text>`,
    );
  }
  if (subtitle) {
    parts.push(`<text x="${rect.x + padX}" y="${rect.y + 46}" fill="#b7c8de" font-size="11">${subtitle}</text>`);
  }
  if (detail) {
    parts.push(`<text x="${rect.x + padX}" y="${rect.y + 62}" fill="#8292aa" font-size="10">${detail}</text>`);
  }
  if (typeof node.data.executionOrder === 'number') {
    parts.push(
      `<circle cx="${rect.x}" cy="${rect.y}" r="11" fill="${escapeXml(visual.accent)}" stroke="#07101c" stroke-width="2" />` +
        `<text x="${rect.x}" y="${rect.y + 4}" fill="#07101c" font-size="11" font-weight="800" text-anchor="middle">${node.data.executionOrder}</text>`,
    );
  }
  return parts.join('');
}

function exportStepPath(source: ExportRect, target: ExportRect): string {
  const sx = source.x + source.w / 2;
  const sy = source.y + source.h;
  const tx = target.x + target.w / 2;
  const ty = target.y;
  const midY = (sy + ty) / 2;
  return `M${sx},${sy} L${sx},${midY} L${tx},${midY} L${tx},${ty}`;
}

function parsePathPoints(path: string): { x: number; y: number }[] {
  const points: { x: number; y: number }[] = [];
  const matches = path.matchAll(/[ML]\s*(-?\d+(?:\.\d+)?),(-?\d+(?:\.\d+)?)/g);
  for (const match of matches) {
    points.push({ x: Number(match[1]), y: Number(match[2]) });
  }
  return points;
}

function truncateText(value: string, maxChars: number): string {
  const text = value.trim();
  if (text.length <= maxChars) {
    return text;
  }
  return `${text.slice(0, Math.max(1, maxChars - 1))}…`;
}

function escapeXml(value: string): string {
  return value
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&apos;');
}

function classifyNodeVisualKind(type: string | undefined): 'event' | 'function' | 'macro' | 'construction' | undefined {
  const normalized = (type ?? '').toLowerCase();
  if (normalized.includes('construction')) {
    return 'construction';
  }
  if (normalized.includes('macro')) {
    return 'macro';
  }
  if (normalized.includes('function')) {
    return 'function';
  }
  if (normalized.includes('event')) {
    return 'event';
  }
  return undefined;
}

function getSemanticVisualOverride(
  visual: BPQANodeVisual,
  kind: 'event' | 'function' | 'macro' | 'construction',
  runtimeEventColor: string,
  runtimeFunctionColor: string,
): BPQANodeVisual {
  if (kind !== 'event' && kind !== 'function') {
    return visual;
  }

  const accent = kind === 'event' ? runtimeEventColor : runtimeFunctionColor;
  if (visual.accent === accent && visual.variant === kind) {
    return visual;
  }

  return {
    ...visual,
    variant: kind,
    accent,
    glow: alphaColor(accent, 0.58),
    border: accent,
  };
}

function alphaColor(value: string, alpha: number): string {
  const rgb = hexToRgb(value);
  if (!rgb) {
    return `rgba(255, 255, 255, ${alpha})`;
  }

  return `rgba(${rgb.r}, ${rgb.g}, ${rgb.b}, ${alpha})`;
}

function hexToRgb(value: string): { r: number; g: number; b: number } | undefined {
  const normalized = value.trim().replace('#', '');
  if (normalized.length === 3) {
    const r = Number.parseInt(normalized[0] + normalized[0], 16);
    const g = Number.parseInt(normalized[1] + normalized[1], 16);
    const b = Number.parseInt(normalized[2] + normalized[2], 16);
    return Number.isFinite(r) && Number.isFinite(g) && Number.isFinite(b) ? { r, g, b } : undefined;
  }

  if (normalized.length !== 6) {
    return undefined;
  }

  const r = Number.parseInt(normalized.slice(0, 2), 16);
  const g = Number.parseInt(normalized.slice(2, 4), 16);
  const b = Number.parseInt(normalized.slice(4, 6), 16);
  return Number.isFinite(r) && Number.isFinite(g) && Number.isFinite(b) ? { r, g, b } : undefined;
}

function getMiniMapNodeColor(node: ReactFlowNode): string {
  if (node.type === 'lane') {
    return '#2e394c';
  }

  if (node.type === 'annotation') {
    return '#69778f';
  }

  const data = node.data as { visual?: { accent?: string } } | undefined;
  return data?.visual?.accent ?? '#69a9ff';
}

function getMiniMapNodeStrokeColor(node: ReactFlowNode): string {
  return node.selected ? '#ffd65a' : '#0a0f18';
}

function getModeLabel(mode: BPQAViewMode): string {
  switch (mode) {
    case 'RuntimeFlow':
      return '运行流程视图';
    case 'NodeFlow':
      return '类内节点流程';
    case 'FolderDependency':
      return '文件夹依赖';
    case 'FolderAssetDependency':
      return '文件夹资产依赖';
    case 'StaticDependency':
      return '扫描静态依赖';
    default:
      return 'Blueprint Query Analysis';
  }
}

function getMotionShapeLabel(shape: string): string {
  switch (shape) {
    case 'orb':
      return '光球';
    case 'box':
      return '方块';
    case 'diamond':
      return '菱形';
    case 'triangle':
      return '三角';
    case 'star':
      return '星形';
    case 'package':
      return '立方包';
    case 'custom':
      return '自定义 SVG';
    default:
      return shape;
  }
}

function sanitizeFileName(value: string): string {
  return value.replace(/[\\/:*?"<>|]+/g, '_').trim() || 'BlueprintQueryAnalysis';
}

function isStaticPanelMode(mode: BPQAViewMode): boolean {
  return mode === 'StaticDependency' || mode === 'FolderDependency' || mode === 'FolderAssetDependency';
}

function clampGridGap(value: number): number {
  if (!Number.isFinite(value)) {
    return DEFAULT_GRID_GAP;
  }

  return Math.min(MAX_GRID_GAP, Math.max(MIN_GRID_GAP, Math.round(value)));
}

function clampNumber(value: number, min: number, max: number, fallback: number): number {
  if (!Number.isFinite(value)) {
    return fallback;
  }

  return Math.min(max, Math.max(min, value));
}

function snapValue(value: number, gridGap: number): number {
  const safeGap = clampGridGap(gridGap);
  return Math.round(value / safeGap) * safeGap;
}

function snapZoom(value: number, gridGap: number): number {
  const step = Math.max(0.01, clampGridGap(gridGap) / 200);
  const snapped = Math.round(value / step) * step;
  return Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, snapped));
}

function snapViewport(viewport: Viewport, gridGap: number): Viewport {
  return {
    x: snapValue(viewport.x, gridGap),
    y: snapValue(viewport.y, gridGap),
    zoom: snapZoom(viewport.zoom, gridGap),
  };
}

function isSameViewport(left: Viewport, right: Viewport): boolean {
  return (
    Math.abs(left.x - right.x) < 0.01 &&
    Math.abs(left.y - right.y) < 0.01 &&
    Math.abs(left.zoom - right.zoom) < 0.001
  );
}

function StateOverlay(props: { overlay: { title: string; detail: string; progress?: number; error?: boolean } }) {
  return (
    <div className={`bpqa-state ${props.overlay.error ? 'bpqa-state--error' : ''}`}>
      {props.overlay.progress !== undefined ? <div className="bpqa-loader" /> : null}
      <div className="bpqa-state__title">{props.overlay.title}</div>
      <div className="bpqa-state__detail">{props.overlay.detail}</div>
      {props.overlay.progress !== undefined ? (
        <div className="bpqa-progress">
          <span style={{ width: `${Math.round(props.overlay.progress * 100)}%` }} />
        </div>
      ) : null}
    </div>
  );
}

function makeOverlay(viewState: ViewState, mode: string, nodeCount: number) {
  if (viewState.kind === 'error') {
    return {
      title: '前端视图错误',
      detail: viewState.message,
      error: true,
    };
  }

  if (viewState.kind === 'loading') {
    return {
      title: viewState.message || '正在加载图数据',
      detail: '正在读取本地项目数据并更新 React Flow 视图。',
      progress: viewState.progress,
    };
  }

  if (nodeCount > 0) {
    return null;
  }

  if (mode === 'RuntimeFlow') {
    return {
      title: '需要运行 PIE 或运行一次以获得运行流程数据',
      detail: '进入 PIE 并触发蓝图事件或函数后，运行流程树会显示在这里。',
    };
  }

  if (mode === 'NodeFlow') {
    return {
      title: '需要选择蓝图并生成类内节点流程',
      detail: '类内 Ubergraph / FunctionGraph 分析会显示在这里。',
    };
  }

  return {
    title: '需要执行扫描静态依赖',
    detail: '点击左侧“扫描静态依赖”，读取 /All/Game 对应的本地项目内容。',
  };
}

function isSyntheticNode(nodeId: string): boolean {
  return nodeId.startsWith('__');
}

function makeGraphContentSignature(graph: BPQAGraphPayload | undefined): string {
  const safeGraph = graph ?? EMPTY_STATIC_GRAPH;
  const nodeIds = (safeGraph.nodes ?? [])
    .map(
      (node) =>
        `${node.id}:${node.label ?? ''}:${node.type ?? ''}:${node.depth ?? 0}:${node.assetPath ?? ''}:${node.packageName ?? ''}`,
    )
    .join('|');
  const edgeIds = (safeGraph.edges ?? [])
    .map((edge) => `${edge.from}>${edge.to}:${edge.type ?? ''}:${edge.reason ?? ''}`)
    .join('|');
  return `${safeGraph.mode ?? 'StaticDependency'}:${safeGraph.title ?? ''}:${nodeIds}:${edgeIds}`;
}
