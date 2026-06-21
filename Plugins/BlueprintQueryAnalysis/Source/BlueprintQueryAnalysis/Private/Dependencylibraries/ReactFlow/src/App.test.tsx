import { act, render, screen, waitFor, within } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';
import { BlueprintQueryAnalysisApp } from './App';
import * as graphMapper from './graphMapper';

describe('BlueprintQueryAnalysisApp', () => {
  function installResizeObserverMock() {
    class ResizeObserverMock {
      observe() {}
      unobserve() {}
      disconnect() {}
    }
    window.ResizeObserver = ResizeObserverMock;
  }

  it('renders editable runtime color controls', async () => {
    installResizeObserverMock();

    render(<BlueprintQueryAnalysisApp />);

    await act(async () => {
      window.BPQASetGraph?.({
        mode: 'RuntimeFlow',
        nodes: [{ id: 'Level', label: 'CurrentMap', type: 'RuntimeRoot', depth: 0 }],
        edges: [],
      });
    });

    await waitFor(() => expect(screen.getByText('运行流程视图')).toBeInTheDocument());

    expect(screen.getByText('传送带颜色').closest('label')?.querySelector('input[type="color"]')).toBeInTheDocument();
    expect(screen.getByText('事件颜色').closest('label')?.querySelector('input[type="color"]')).toBeInTheDocument();
    expect(screen.getByText('函数颜色').closest('label')?.querySelector('input[type="color"]')).toBeInTheDocument();
  });

  it('exposes the manual refresh, orthogonal routing and motion shape controls', async () => {
    installResizeObserverMock();

    // Scope queries to this render's container; tests in this file do not share
    // an auto-cleanup afterEach, so the document accumulates earlier renders.
    const { container } = render(<BlueprintQueryAnalysisApp />);
    const view = within(container);

    await act(async () => {
      window.BPQASetGraph?.({
        mode: 'RuntimeFlow',
        nodes: [
          { id: 'Level', label: 'CurrentMap', type: 'RuntimeRoot', depth: 0 },
          { id: 'BeginPlay', label: 'BeginPlay', type: 'BlueprintEvent', depth: 1 },
        ],
        edges: [{ from: 'Level', to: 'BeginPlay', type: 'RuntimeOrder' }],
      });
    });

    await waitFor(() => expect(view.getByText('刷新视图')).toBeInTheDocument());
    expect(view.getByText('正交避让布线').closest('label')?.querySelector('input[type="checkbox"]')).toBeInTheDocument();

    const shapeSelect = view.getByText('物体形状').closest('label')?.querySelector('select');
    expect(shapeSelect).toBeInTheDocument();

    // Choosing the custom shape reveals the custom SVG path input.
    expect(view.queryByText('自定义路径')).not.toBeInTheDocument();
    await act(async () => {
      if (shapeSelect) {
        shapeSelect.value = 'custom';
        shapeSelect.dispatchEvent(new Event('change', { bubbles: true }));
      }
    });
    await waitFor(() => expect(view.getByText('自定义路径')).toBeInTheDocument());
  });

  it('does not remap runtime edges when only the selected node changes', async () => {
    installResizeObserverMock();
    const mapSpy = vi.spyOn(graphMapper, 'mapGraphToReactFlow');

    render(<BlueprintQueryAnalysisApp />);

    await act(async () => {
      window.BPQASetGraph?.({
        mode: 'RuntimeFlow',
        nodes: [
          { id: 'Level', label: 'CurrentMap', type: 'RuntimeRoot', depth: 0 },
          { id: 'BeginPlay', label: 'BeginPlay', type: 'BlueprintEvent', depth: 1 },
        ],
        edges: [{ from: 'Level', to: 'BeginPlay', type: 'RuntimeOrder' }],
      });
    });

    await waitFor(() => expect(screen.getAllByText('BeginPlay').length).toBeGreaterThan(0));
    const callCountAfterGraph = mapSpy.mock.calls.length;

    await act(async () => {
      window.BPQASetSelected?.('BeginPlay');
    });

    expect(mapSpy).toHaveBeenCalledTimes(callCountAfterGraph);
  });
});
