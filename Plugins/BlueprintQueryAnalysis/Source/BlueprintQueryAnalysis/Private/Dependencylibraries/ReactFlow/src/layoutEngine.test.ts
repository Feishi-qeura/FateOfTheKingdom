import { describe, expect, it } from 'vitest';
import { mapGraphToReactFlow } from './graphMapper';
import { computeLayout, isElkMode, isForceMode } from './layoutEngine';
import type { BPQAGraphPayload } from './types';

const runtimePayload: BPQAGraphPayload = {
  mode: 'RuntimeFlow',
  nodes: [
    { id: 'Level', label: 'CurrentMap', type: 'RuntimeRoot', depth: 0 },
    { id: 'BeginPlay', label: 'BeginPlay', type: 'BlueprintEvent', depth: 1 },
    { id: 'Spawn', label: 'SpawnActor', type: 'BlueprintFunction', depth: 2 },
  ],
  edges: [
    { from: 'Level', to: 'BeginPlay', type: 'RuntimeOrder' },
    { from: 'BeginPlay', to: 'Spawn', type: 'RuntimeOrder' },
  ],
};

const staticPayload: BPQAGraphPayload = {
  mode: 'StaticDependency',
  nodes: [
    { id: 'A', label: 'A' },
    { id: 'B', label: 'B' },
    { id: 'C', label: 'C' },
  ],
  edges: [
    { from: 'A', to: 'B' },
    { from: 'A', to: 'C' },
  ],
};

describe('layout mode predicates', () => {
  it('routes runtime/node flow to ELK and static/folder to force', () => {
    expect(isElkMode('RuntimeFlow')).toBe(true);
    expect(isElkMode('NodeFlow')).toBe(true);
    expect(isForceMode('StaticDependency')).toBe(true);
    expect(isForceMode('FolderDependency')).toBe(true);
    expect(isElkMode('StaticDependency')).toBe(false);
  });
});

describe('computeLayout', () => {
  it('produces ELK layered positions for runtime flow', async () => {
    const mapped = mapGraphToReactFlow(runtimePayload);
    const result = await computeLayout('RuntimeFlow', mapped.nodes, mapped.edges);
    expect(result.engine).toBe('elk');
    expect(result.flatten).toBe(true);
    for (const id of ['Level', 'BeginPlay', 'Spawn']) {
      expect(result.positions[id]).toBeDefined();
      expect(Number.isFinite(result.positions[id].x)).toBe(true);
      expect(Number.isFinite(result.positions[id].y)).toBe(true);
    }
    // Layered DOWN layout: each successive depth sits lower than the previous.
    expect(result.positions.BeginPlay.y).toBeGreaterThan(result.positions.Level.y);
    expect(result.positions.Spawn.y).toBeGreaterThan(result.positions.BeginPlay.y);
  });

  it('produces a finite d3-force layout for static dependency', async () => {
    const mapped = mapGraphToReactFlow(staticPayload);
    const result = await computeLayout('StaticDependency', mapped.nodes, mapped.edges);
    expect(result.engine).toBe('d3');
    expect(result.flatten).toBe(true);
    for (const id of ['A', 'B', 'C']) {
      expect(result.positions[id]).toBeDefined();
      expect(Number.isFinite(result.positions[id].x)).toBe(true);
      expect(Number.isFinite(result.positions[id].y)).toBe(true);
    }
    // The simulation must separate the nodes rather than collapsing them.
    const distinct = new Set(Object.values(result.positions).map((point) => `${Math.round(point.x)}:${Math.round(point.y)}`));
    expect(distinct.size).toBe(3);
  });

  it('returns no layout for an empty graph', async () => {
    const result = await computeLayout('RuntimeFlow', [], []);
    expect(result.engine).toBe('none');
  });
});
