import { render } from '@testing-library/react';
import { ReactFlowProvider } from '@xyflow/react';
import { describe, expect, it } from 'vitest';
import { BPQANode } from './BPQANode';

describe('BPQANode', () => {
  it('marks connected runtime nodes for turbo-style flow animation', () => {
    const { container } = render(
      <ReactFlowProvider>
        <BPQANode
          id="runtime-1"
          type="bpqaNode"
          selected={false}
          selectable
          deletable={false}
          draggable
          dragging={false}
          zIndex={2}
          isConnectable={false}
          positionAbsoluteX={0}
          positionAbsoluteY={0}
          data={{
            title: 'BeginPlay',
            subtitle: 'BlueprintEvent',
            detail: '/Game/BP_Test',
            connected: true,
            compact: true,
            viewMode: 'RuntimeFlow',
            visual: {
              variant: 'blueprint',
              accent: '#3aa7ff',
              glow: 'rgba(58, 167, 255, 0.58)',
              background: '#0c1d32',
              foreground: '#eef6ff',
              border: '#3aa7ff',
            },
          }}
        />
      </ReactFlowProvider>,
    );

    expect(container.querySelector('.bpqa-node')).toHaveClass('bpqa-node--runtime-flow');
  });

  it('renders execution order badge in the top-left corner', () => {
    const { container } = render(
      <ReactFlowProvider>
        <BPQANode
          id="runtime-2"
          type="bpqaNode"
          selected={false}
          selectable
          deletable={false}
          draggable
          dragging={false}
          zIndex={2}
          isConnectable={false}
          positionAbsoluteX={0}
          positionAbsoluteY={0}
          data={{
            title: 'Call Function',
            subtitle: 'BlueprintFunction',
            detail: '/Game/BP_Test',
            connected: true,
            compact: true,
            viewMode: 'RuntimeFlow',
            executionOrder: 7,
            visual: {
              variant: 'function',
              accent: '#5ea7ff',
              glow: 'rgba(94, 167, 255, 0.58)',
              background: '#0c1d32',
              foreground: '#eef6ff',
              border: '#5ea7ff',
            },
          }}
        />
      </ReactFlowProvider>,
    );

    expect(container.querySelector('.bpqa-node__order')).toHaveTextContent('7');
  });
});
