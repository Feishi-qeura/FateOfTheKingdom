import { describe, expect, it } from 'vitest';
import { mapGraphToReactFlow } from './graphMapper';
import { getNodeVisual } from './styleRules';

describe('mapGraphToReactFlow', () => {
  it('maps BPQA graph data into React Flow nodes and edges', () => {
    const graph = mapGraphToReactFlow({
      title: 'Static',
      mode: 'StaticDependency',
      nodes: [
        { id: '/Game/A', label: 'A', type: 'Blueprint', isBlueprint: true, depth: 0 },
        { id: '/Game/B', label: 'B', type: 'Material', depth: 1 },
      ],
      edges: [{ from: '/Game/A', to: '/Game/B', reason: 'HardReference', type: 'AssetDependency' }],
    });

    expect(graph.nodes.filter((node) => node.type === 'bpqaNode')).toHaveLength(2);
    expect(graph.edges).toHaveLength(1);
    expect(graph.edges[0].type).toBe('step');
    expect(graph.nodes.find((node) => node.id === '/Game/A')?.draggable).toBe(true);
  });

  it('places disconnected assets inside the orphan annotation group', () => {
    const graph = mapGraphToReactFlow({
      mode: 'StaticDependency',
      nodes: [{ id: 'A', label: 'A', type: 'Material' }],
      edges: [],
    });

    expect(graph.nodes.some((node) => node.id === '__inactive_group')).toBe(true);
    expect(graph.nodes.find((node) => node.id === 'A')?.parentId).toBe('__inactive_group');
  });

  it('keeps folder nodes in the main tree even without edges', () => {
    const graph = mapGraphToReactFlow({
      mode: 'FolderDependency',
      nodes: [
        { id: 'Folder:/Game/X', label: 'X', type: 'Folder' },
        { id: 'Folder:/Game/Y', label: 'Y', type: 'Folder' },
      ],
      edges: [],
    });

    expect(graph.nodes.some((node) => node.id === '__inactive_group')).toBe(false);
    expect(graph.nodes.find((node) => node.id === 'Folder:/Game/X')?.parentId).toBeUndefined();
  });

  it('keeps explicit folder overview children on the same tree level', () => {
    const graph = mapGraphToReactFlow({
      mode: 'FolderDependency',
      nodes: [
        { id: 'Folder:/Game/Project', label: 'Project', type: 'Folder', depth: 0 },
        { id: 'Folder:/Game/Project/A', label: 'A', type: 'Folder', depth: 1 },
        { id: 'Folder:/Game/Project/B', label: 'B', type: 'Folder', depth: 1 },
      ],
      edges: [
        { from: 'Folder:/Game/Project', to: 'Folder:/Game/Project/A', type: 'FolderContains' },
        { from: 'Folder:/Game/Project', to: 'Folder:/Game/Project/B', type: 'FolderContains' },
        { from: 'Folder:/Game/Project/A', to: 'Folder:/Game/Project/B', type: 'FolderDependency' },
      ],
    });

    const childA = graph.nodes.find((node) => node.id === 'Folder:/Game/Project/A');
    const childB = graph.nodes.find((node) => node.id === 'Folder:/Game/Project/B');

    expect(childA?.position.y).toBe(childB?.position.y);
  });

  it('uses a top-down layered static tree layout with parents above their children', () => {
    const graph = mapGraphToReactFlow({
      mode: 'StaticDependency',
      nodes: [
        { id: 'Root', label: 'Root', type: 'Folder', depth: 0 },
        { id: 'ChildA', label: 'ChildA', type: 'Blueprint', depth: 1 },
        { id: 'ChildB', label: 'ChildB', type: 'Blueprint', depth: 1 },
      ],
      edges: [
        { from: 'Root', to: 'ChildA', type: 'FolderContains' },
        { from: 'Root', to: 'ChildB', type: 'FolderContains' },
      ],
    });

    const root = graph.nodes.find((node) => node.id === 'Root');
    const childA = graph.nodes.find((node) => node.id === 'ChildA');
    const childB = graph.nodes.find((node) => node.id === 'ChildB');

    expect(root?.position.y).toBeLessThan(childA?.position.y ?? 0);
    expect(childA?.position.y).toBe(childB?.position.y);
    expect(root?.sourcePosition).toBe('bottom');
    expect(childA?.targetPosition).toBe('top');
  });

  it('separates connected assets (tree) from orphan assets (group)', () => {
    const graph = mapGraphToReactFlow({
      mode: 'FolderAssetDependency',
      nodes: [
        { id: '/Game/A', label: 'A', type: 'Blueprint', isBlueprint: true },
        { id: '/Game/B', label: 'B', type: 'Material' },
        { id: '/Game/C', label: 'C', type: 'Texture' },
      ],
      edges: [{ from: '/Game/A', to: '/Game/B', type: 'AssetDependency' }],
    });

    expect(graph.nodes.find((node) => node.id === '/Game/A')?.parentId).toBeUndefined();
    expect(graph.nodes.find((node) => node.id === '/Game/B')?.parentId).toBeUndefined();
    expect(graph.nodes.find((node) => node.id === '/Game/C')?.parentId).toBe('__inactive_group');
  });

  it('collapses oversized static trees by default and hides edges attached to folded nodes', () => {
    const nodes = Array.from({ length: 126 }, (_, index) => ({
      id: `/Game/Asset${index}`,
      label: `Asset${index}`,
      type: 'Blueprint',
    }));
    const edges = nodes.slice(0, -1).map((node, index) => ({
      from: node.id,
      to: nodes[index + 1].id,
      type: 'AssetDependency',
    }));

    const graph = mapGraphToReactFlow({
      mode: 'StaticDependency',
      nodes,
      edges,
    });

    expect(graph.nodes.some((node) => node.id === '__static_tree_fold')).toBe(true);
    expect(graph.nodes.filter((node) => node.type === 'bpqaNode' && node.hidden)).not.toHaveLength(0);
    expect(graph.edges.filter((edge) => edge.hidden)).not.toHaveLength(0);
  });

  it('places the collapsed tree annotation away from the orphan asset group', () => {
    const nodes = Array.from({ length: 126 }, (_, index) => ({
      id: `/Game/Orphan${index}`,
      label: `Orphan${index}`,
      type: 'Blueprint',
    }));

    const graph = mapGraphToReactFlow({
      mode: 'StaticDependency',
      nodes,
      edges: [],
    });

    const folded = graph.nodes.find((node) => node.id === '__static_tree_fold');
    const orphanGroup = graph.nodes.find((node) => node.id === '__inactive_group');
    const orphanHeight = typeof orphanGroup?.style?.height === 'number' ? orphanGroup.style.height : 0;

    expect(folded?.position.y ?? 0).toBeGreaterThan((orphanGroup?.position.y ?? 0) + orphanHeight);
  });

  it('assigns separate routing lanes to long static dependency edges', () => {
    const graph = mapGraphToReactFlow({
      mode: 'StaticDependency',
      nodes: [
        { id: 'Root', label: 'Root', type: 'Folder', depth: 0 },
        { id: 'A', label: 'A', type: 'Blueprint', depth: 1 },
        { id: 'B', label: 'B', type: 'Blueprint', depth: 1 },
        { id: 'C', label: 'C', type: 'Blueprint', depth: 1 },
      ],
      edges: [
        { from: 'Root', to: 'A', type: 'FolderContains' },
        { from: 'Root', to: 'B', type: 'FolderContains' },
        { from: 'Root', to: 'C', type: 'FolderContains' },
      ],
    });

    const lanes = new Set(graph.edges.map((edge) => edge.data?.routeLane));
    expect(lanes.size).toBe(graph.edges.length);
  });

  it('can expand oversized static trees without rebuilding scan data', () => {
    const nodes = Array.from({ length: 126 }, (_, index) => ({
      id: `/Game/Asset${index}`,
      label: `Asset${index}`,
      type: 'Blueprint',
    }));
    const edges = nodes.slice(0, -1).map((node, index) => ({
      from: node.id,
      to: nodes[index + 1].id,
      type: 'AssetDependency',
    }));

    const graph = mapGraphToReactFlow(
      {
        mode: 'StaticDependency',
        nodes,
        edges,
      },
      '',
      { collapseLargeStaticTree: false },
    );

    expect(graph.nodes.some((node) => node.id === '__static_tree_fold')).toBe(false);
    expect(graph.nodes.filter((node) => node.type === 'bpqaNode' && node.hidden)).toHaveLength(0);
    expect(graph.edges.filter((edge) => edge.hidden)).toHaveLength(0);
  });

  it('staggers mutually calling runtime nodes inside the same explicit layer', () => {
    const graph = mapGraphToReactFlow({
      mode: 'RuntimeFlow',
      nodes: [
        { id: 'Level', label: 'CurrentMap', type: 'RuntimeRoot', depth: 0, timeSeconds: 0 },
        { id: 'EventA', label: 'EventA', type: 'BlueprintEvent', depth: 1, timeSeconds: 1 },
        { id: 'EventB', label: 'EventB', type: 'BlueprintEvent', depth: 1, timeSeconds: 2 },
      ],
      edges: [
        { from: 'Level', to: 'EventA', type: 'RuntimeOrder' },
        { from: 'EventA', to: 'EventB', type: 'BlueprintCall' },
        { from: 'EventB', to: 'EventA', type: 'BlueprintCall' },
      ],
    });

    const eventA = graph.nodes.find((node) => node.id === 'EventA');
    const eventB = graph.nodes.find((node) => node.id === 'EventB');

    expect(eventA?.position.y).not.toBe(eventB?.position.y);
    expect(eventA?.data.viewMode).toBe('RuntimeFlow');
  });

  it('assigns separated side-road lanes and offsets to same-layer runtime edges', () => {
    const graph = mapGraphToReactFlow({
      mode: 'RuntimeFlow',
      nodes: [
        { id: 'Level', label: 'CurrentMap', type: 'RuntimeRoot', depth: 0, timeSeconds: 0 },
        { id: 'EventA', label: 'EventA', type: 'BlueprintEvent', depth: 1, timeSeconds: 1 },
        { id: 'EventB', label: 'EventB', type: 'BlueprintEvent', depth: 1, timeSeconds: 2 },
        { id: 'EventC', label: 'EventC', type: 'BlueprintEvent', depth: 1, timeSeconds: 3 },
      ],
      edges: [
        { from: 'EventA', to: 'EventB', type: 'BlueprintCall' },
        { from: 'EventB', to: 'EventC', type: 'BlueprintCall' },
        { from: 'EventC', to: 'EventA', type: 'BlueprintCall' },
      ],
    });

    const routeLanes = new Set(graph.edges.map((edge) => edge.data?.routeLane));
    const routeOffsets = new Set(graph.edges.map((edge) => edge.data?.routeOffset));

    expect(routeLanes.size).toBe(graph.edges.length);
    expect(routeOffsets.has(0)).toBe(false);
  });

  it('keeps a single runtime edge on the normal route instead of forcing a side road', () => {
    const graph = mapGraphToReactFlow({
      mode: 'RuntimeFlow',
      nodes: [
        { id: 'Level', label: 'CurrentMap', type: 'RuntimeRoot', depth: 0, timeSeconds: 0 },
        { id: 'BeginPlay', label: 'BeginPlay', type: 'BlueprintEvent', depth: 1, timeSeconds: 1 },
      ],
      edges: [{ from: 'Level', to: 'BeginPlay', type: 'RuntimeOrder' }],
    });

    expect(graph.edges).toHaveLength(1);
    expect(graph.edges[0].data?.routeOffset).toBe(0);
  });

  it('routes single same-layer node-flow edges through a side road', () => {
    const graph = mapGraphToReactFlow({
      mode: 'NodeFlow',
      nodes: [
        { id: 'EventA', label: 'EventA', type: 'BlueprintEvent', depth: 1, timeSeconds: 1 },
        { id: 'EventB', label: 'EventB', type: 'BlueprintFunction', depth: 1, timeSeconds: 2 },
      ],
      edges: [{ from: 'EventA', to: 'EventB', type: 'BlueprintExec', shape: 'box', duration: 2 }],
    });

    expect(graph.edges).toHaveLength(1);
    expect(graph.edges[0].data?.routeOffset).not.toBe(0);
    expect(graph.edges[0].data?.shape).toBe('box');
    expect(graph.edges[0].data?.duration).toBe(2);
  });

  it('creates reference tree lanes for runtime flow layers', () => {
    const graph = mapGraphToReactFlow({
      mode: 'RuntimeFlow',
      nodes: [
        { id: 'Level', label: 'CurrentMap', type: 'RuntimeRoot', depth: 0 },
        { id: 'GameModeBeginPlay', label: 'GameMode BeginPlay', type: 'BlueprintEvent', depth: 1 },
        { id: 'ActorBeginPlay', label: 'Actor BeginPlay', type: 'BlueprintEvent', depth: 2 },
        { id: 'GameInstance', label: 'GameInstance', type: 'BlueprintFunction', depth: 3 },
      ],
      edges: [],
    });

    expect(graph.nodes.some((node) => node.id === '__runtime_lane_root')).toBe(true);
    expect(graph.nodes.some((node) => node.id === '__runtime_lane_trunk')).toBe(true);
    expect(graph.nodes.some((node) => node.id === '__runtime_lane_branch')).toBe(true);
    expect(graph.nodes.some((node) => node.id === '__runtime_lane_static')).toBe(true);
    expect(graph.nodes.find((node) => node.id === 'GameModeBeginPlay')?.parentId).toBe('__runtime_lane_trunk');
    expect(graph.nodes.find((node) => node.id === 'GameInstance')?.parentId).toBe('__runtime_lane_static');
  });

  it('adds execution order numbers to runtime and node flow nodes only', () => {
    const runtimeGraph = mapGraphToReactFlow({
      mode: 'RuntimeFlow',
      nodes: [
        { id: 'Level', label: 'CurrentMap', type: 'RuntimeRoot', depth: 0, timeSeconds: 0 },
        { id: 'Second', label: 'Second', type: 'BlueprintEvent', depth: 1, timeSeconds: 2 },
        { id: 'First', label: 'First', type: 'BlueprintEvent', depth: 1, timeSeconds: 1 },
      ],
      edges: [],
    });
    const nodeFlowGraph = mapGraphToReactFlow({
      mode: 'NodeFlow',
      nodes: [
        { id: 'CallB', label: 'CallB', type: 'BlueprintFunction', timeSeconds: 2 },
        { id: 'CallA', label: 'CallA', type: 'BlueprintEvent', timeSeconds: 1 },
      ],
      edges: [],
    });
    const staticGraph = mapGraphToReactFlow({
      mode: 'StaticDependency',
      nodes: [{ id: 'Asset', label: 'Asset', type: 'Blueprint' }],
      edges: [],
    });

    expect(runtimeGraph.nodes.find((node) => node.id === 'First')?.data.executionOrder).toBe(2);
    expect(runtimeGraph.nodes.find((node) => node.id === 'Second')?.data.executionOrder).toBe(3);
    expect(nodeFlowGraph.nodes.find((node) => node.id === 'CallA')?.data.executionOrder).toBe(1);
    expect(staticGraph.nodes.find((node) => node.id === 'Asset')?.data.executionOrder).toBeUndefined();
  });
});

describe('getNodeVisual', () => {
  it('classifies inactive, blueprint and generic asset nodes', () => {
    expect(getNodeVisual({ id: 'A', type: 'Blueprint' }, false).variant).toBe('inactive');
    expect(getNodeVisual({ id: 'A', type: 'Blueprint' }, true).variant).toBe('blueprint');
    expect(getNodeVisual({ id: 'B', type: 'Material' }, true).variant).toBe('asset');
  });

  it('renders unexecuted runtime candidates as inactive even when connected', () => {
    expect(
      getNodeVisual({ id: 'A', type: 'BlueprintEvent', components: ['ExecutionState:Inactive'] }, true, 'RuntimeFlow').variant,
    ).toBe('inactive');
  });

  it('always renders folders with the folder variant regardless of connection', () => {
    expect(getNodeVisual({ id: 'Folder:/Game/X', type: 'Folder' }, false).variant).toBe('folder');
    expect(getNodeVisual({ id: 'Folder:/Game/X', type: 'Folder' }, true).variant).toBe('folder');
  });
});
