import type { CSSProperties } from 'react';
import { Handle, Position, type NodeProps } from '@xyflow/react';
import type { BPQAFlowNode, BPQAFlowNodeData } from './types';

export function BPQANode(props: NodeProps<BPQAFlowNode>) {
  const data = props.data as BPQAFlowNodeData;
  const visual = data.visual;
  const className = [
    'bpqa-node',
    `bpqa-node--${visual.variant}`,
    data.compact ? 'bpqa-node--compact' : '',
    data.viewMode === 'RuntimeFlow' && data.connected && visual.variant !== 'inactive' ? 'bpqa-node--runtime-flow' : '',
    props.selected ? 'bpqa-node--selected' : '',
  ]
    .filter(Boolean)
    .join(' ');

  return (
    <div
      className={className}
      style={
        {
          '--bpqa-node-accent': visual.accent,
          '--bpqa-node-glow': visual.glow,
          '--bpqa-node-bg': visual.background,
          '--bpqa-node-fg': visual.foreground,
          '--bpqa-node-border': visual.border,
        } as CSSProperties
      }
      title={data.bpqa?.tooltip || data.bpqa?.assetPath || data.bpqa?.packageName || data.title}
    >
      <Handle id="in" type="target" position={Position.Top} className="bpqa-node__handle" />
      {typeof data.executionOrder === 'number' ? (
        <span className="bpqa-node__order" aria-label="Execution order">
          {data.executionOrder}
        </span>
      ) : null}
      <div className="bpqa-node__header">
        <span className="bpqa-node__status" />
        <span className="bpqa-node__title">{data.title}</span>
      </div>
      <div className="bpqa-node__subtitle">{data.subtitle}</div>
      <div className="bpqa-node__detail">{data.detail}</div>
      <Handle id="out" type="source" position={Position.Bottom} className="bpqa-node__handle" />
    </div>
  );
}
