import { render } from '@testing-library/react';
import { Position } from '@xyflow/react';
import { describe, expect, it } from 'vitest';
import { BPQAEdge } from './BPQAEdge';

describe('BPQAEdge', () => {
  it('renders main path, arrow marker, glow path and white flow path as a step edge', () => {
    const { container } = render(
      <svg>
        <BPQAEdge
          id="edge-1"
          source="A"
          target="B"
          sourceX={0}
          sourceY={0}
          targetX={160}
          targetY={40}
          sourcePosition={Position.Right}
          targetPosition={Position.Left}
          selected={false}
          animated={false}
          data={{
            bpqa: { from: 'A', to: 'B', reason: 'Reference' },
            accent: '#38d6bd',
            glow: '#38d6bd',
            label: '',
            viewMode: 'StaticDependency',
          }}
        />
      </svg>,
    );

    expect(container.querySelector('.bpqa-edge-glow')).toBeInTheDocument();
    const mainPath = container.querySelector('.bpqa-edge-main');
    expect(mainPath).toBeInTheDocument();
    const path = mainPath?.getAttribute('d') ?? '';
    expect((path.match(/L/g) ?? [])).toHaveLength(3);
    expect(path).not.toContain('C');
    expect(container.querySelector('.bpqa-edge-flow')).toBeInTheDocument();
    expect(container.querySelector('marker path')).toHaveAttribute('fill', '#38d6bd');
  });

  it('can hide the conveyor track while keeping the moving objects visible', () => {
    const { container } = render(
      <svg>
        <BPQAEdge
          id="edge-2"
          source="A"
          target="B"
          sourceX={0}
          sourceY={0}
          targetX={160}
          targetY={40}
          sourcePosition={Position.Right}
          targetPosition={Position.Left}
          selected={false}
          animated={false}
          data={{
            bpqa: { from: 'A', to: 'B', reason: 'Reference' },
            accent: '#38d6bd',
            glow: '#38d6bd',
            label: '',
            viewMode: 'RuntimeFlow',
            motionTrackVisible: false,
          }}
        />
      </svg>,
    );

    expect(container.querySelector('.bpqa-edge-main')).not.toBeInTheDocument();
    expect(container.querySelector('.bpqa-edge-flow')).not.toBeInTheDocument();
    expect(container.querySelector('.bpqa-edge-orb')).toBeInTheDocument();
  });

  it('renders node-flow conveyor objects with custom svg shapes', () => {
    const { container } = render(
      <svg>
        <BPQAEdge
          id="edge-node-flow"
          source="A"
          target="B"
          sourceX={0}
          sourceY={0}
          targetX={160}
          targetY={40}
          sourcePosition={Position.Right}
          targetPosition={Position.Left}
          selected={false}
          animated={false}
          data={{
            bpqa: { from: 'A', to: 'B', reason: 'Exec' },
            accent: '#38d6bd',
            glow: '#38d6bd',
            label: '',
            viewMode: 'NodeFlow',
            motionTrackColor: '#123456',
            motionColor: '#abcdef',
            shape: 'box',
            duration: 2,
          }}
        />
      </svg>,
    );

    expect(container.querySelector('.bpqa-edge-main')).toHaveAttribute('stroke', '#123456');
    expect(container.querySelector('rect.bpqa-edge-orb')).toHaveAttribute('fill', '#abcdef');
    expect(container.querySelector('animateMotion')).toHaveAttribute('dur', '2s');
    expect(container.querySelector('.bpqa-edge-flow')).not.toBeInTheDocument();
  });

  it('routes vertical runtime edges through side-road bends', () => {
    const { container } = render(
      <svg>
        <BPQAEdge
          id="edge-3"
          source="A"
          target="B"
          sourceX={10}
          sourceY={0}
          targetX={100}
          targetY={200}
          sourcePosition={Position.Bottom}
          targetPosition={Position.Top}
          selected={false}
          animated={false}
          data={{
            bpqa: { from: 'A', to: 'B', reason: 'Call' },
            accent: '#63a9ff',
            glow: '#63a9ff',
            label: '',
            viewMode: 'RuntimeFlow',
            routeLane: 120,
            routeOffset: 160,
          }}
        />
      </svg>,
    );

    const path = container.querySelector('.bpqa-edge-main')?.getAttribute('d') ?? '';
    expect((path.match(/L/g) ?? [])).toHaveLength(5);
    expect(path).toContain('L10,40');
    expect(path).toContain('L260,40');
    expect(path).toContain('L260,160');
    expect(path).toContain('L100,200');
  });

  it('keeps normal vertical edges perpendicular at both node handles', () => {
    const { container } = render(
      <svg>
        <BPQAEdge
          id="edge-4"
          source="A"
          target="B"
          sourceX={10}
          sourceY={0}
          targetX={100}
          targetY={200}
          sourcePosition={Position.Bottom}
          targetPosition={Position.Top}
          selected={false}
          animated={false}
          data={{
            bpqa: { from: 'A', to: 'B', reason: 'Call' },
            accent: '#63a9ff',
            glow: '#63a9ff',
            label: '',
            viewMode: 'RuntimeFlow',
            routeOffset: 0,
          }}
        />
      </svg>,
    );

    const path = container.querySelector('.bpqa-edge-main')?.getAttribute('d') ?? '';
    expect(path).toBe('M10,0 L10,100 L100,100 L100,200');
  });
});
