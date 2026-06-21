import { EdgeLabelRenderer, Position, type EdgeProps } from '@xyflow/react';
import type { ReactNode, SVGProps } from 'react';
import { BPQA_STYLE } from './styleRules';
import type { BPQAFlowEdge, BPQAFlowEdgeData } from './types';

const MIN_SIDE_ROUTE_OFFSET = 128;
const VERTICAL_ENDPOINT_STUB = 40;

export type AnimatedSvgProps = {
  animateMotionProps: SVGProps<SVGAnimateMotionElement>;
  color: string;
  id: string;
  index: number;
};

export type AnimatedSvg = (props: AnimatedSvgProps) => ReactNode;

export const shapes = {
  orb: ({ animateMotionProps, color }: AnimatedSvgProps) => (
    <circle className="bpqa-edge-orb" r="5.5" fill={color}>
      <animateMotion {...animateMotionProps} />
    </circle>
  ),
  circle: ({ animateMotionProps, color }: AnimatedSvgProps) => (
    <circle className="bpqa-edge-orb" r="5.5" fill={color}>
      <animateMotion {...animateMotionProps} />
    </circle>
  ),
  box: ({ animateMotionProps, color }: AnimatedSvgProps) => (
    <rect className="bpqa-edge-orb" width="8" height="8" x="-4" y="-4" rx="1.5" fill={color}>
      <animateMotion {...animateMotionProps} />
    </rect>
  ),
  diamond: ({ animateMotionProps, color }: AnimatedSvgProps) => (
    <path className="bpqa-edge-orb" d="M0,-6 L6,0 L0,6 L-6,0 Z" fill={color}>
      <animateMotion {...animateMotionProps} />
    </path>
  ),
  triangle: ({ animateMotionProps, color }: AnimatedSvgProps) => (
    <path className="bpqa-edge-orb" d="M0,-7 L7,6 L-7,6 Z" fill={color}>
      <animateMotion {...animateMotionProps} />
    </path>
  ),
  star: ({ animateMotionProps, color }: AnimatedSvgProps) => (
    <path
      className="bpqa-edge-orb"
      d="M0,-7 L1.8,-2.2 L7,-2.2 L2.8,1.1 L4.5,6.5 L0,3.2 L-4.5,6.5 L-2.8,1.1 L-7,-2.2 L-1.8,-2.2 Z"
      fill={color}
    >
      <animateMotion {...animateMotionProps} />
    </path>
  ),
  package: ({ animateMotionProps, color }: AnimatedSvgProps) => (
    <g className="bpqa-edge-orb">
      <path d="M-6,-3 L0,-6 L6,-3 L0,0 Z" fill={color} />
      <path d="M-6,-3 L0,0 L0,6 L-6,3 Z" fill={color} opacity="0.78" />
      <path d="M6,-3 L0,0 L0,6 L6,3 Z" fill={color} opacity="0.6" />
      <animateMotion {...animateMotionProps} />
    </g>
  ),
} satisfies Record<string, AnimatedSvg>;

const animatedSvgShapes: Record<string, AnimatedSvg> = { ...shapes };

export function registerBPQAAnimatedSvgShape(name: string, shape: AnimatedSvg): void {
  const safeName = name.trim();
  if (!safeName) {
    return;
  }

  animatedSvgShapes[safeName] = shape;
}

declare global {
  interface Window {
    BPQARegisterAnimatedEdgeShape?: (name: string, shape: AnimatedSvg) => void;
  }
}

if (typeof window !== 'undefined') {
  window.BPQARegisterAnimatedEdgeShape = registerBPQAAnimatedSvgShape;
}

export function BPQAEdge(props: EdgeProps<BPQAFlowEdge>) {
  const data = props.data as BPQAFlowEdgeData | undefined;
  const accent = data?.accent ?? BPQA_STYLE.colors.staticEdge;
  const label = data?.label ?? '';
  const isMotionEdge = data?.viewMode === 'RuntimeFlow' || data?.viewMode === 'NodeFlow';
  const motionColor = data?.motionColor ?? '#ff2f8f';
  const motionTrackVisible = data?.motionTrackVisible ?? true;
  const motionTrackColor = data?.motionTrackColor ?? accent;
  const motionSpeedSeconds = data?.duration ?? data?.motionSpeedSeconds ?? 1.15;
  const motionFrequency = Math.max(1, Math.min(8, Math.round(data?.motionFrequency ?? 3)));
  const MotionShape = getAnimatedSvgShape(data?.shape ?? data?.motionShape, data?.customShapePath);
  // Prefer the pre-computed obstacle-avoiding orthogonal route when present;
  // otherwise fall back to the built-in step routing from React Flow geometry.
  let path: string;
  let labelX: number;
  let labelY: number;
  if (data?.routePath) {
    path = data.routePath;
    labelX = data.routeLabelX ?? (props.sourceX + props.targetX) / 2;
    labelY = data.routeLabelY ?? (props.sourceY + props.targetY) / 2;
  } else {
    [path, labelX, labelY] = getRoutedStepPath({
      sourceX: props.sourceX,
      sourceY: props.sourceY,
      sourcePosition: props.sourcePosition,
      targetX: props.targetX,
      targetY: props.targetY,
      targetPosition: props.targetPosition,
      routeOffset: data?.routeOffset ?? 0,
      routeLane: data?.routeLane,
    });
  }
  const safeId = props.id.replace(/[^a-zA-Z0-9_-]/g, '_');
  const markerId = `bpqa-arrow-${safeId}`;
  const glowId = `bpqa-glow-${safeId}`;
  const showTrack = !isMotionEdge || motionTrackVisible;

  return (
    <>
      <defs>
        <filter id={glowId} x="-40%" y="-40%" width="180%" height="180%">
          <feGaussianBlur stdDeviation="3.4" result="blur" />
          <feMerge>
            <feMergeNode in="blur" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
        <marker id={markerId} markerWidth="12" markerHeight="12" refX="10" refY="6" orient="auto" markerUnits="strokeWidth">
          <path d="M 1 1 L 11 6 L 1 11 z" fill={isMotionEdge ? motionTrackColor : accent} />
        </marker>
      </defs>
      {showTrack ? (
        <path
          className="bpqa-edge-glow"
          d={path}
          fill="none"
          stroke={isMotionEdge ? motionTrackColor : accent}
          strokeWidth={BPQA_STYLE.edge.glowWidth}
          filter={`url(#${glowId})`}
        />
      ) : null}
      {showTrack ? (
        <path
          className={`bpqa-edge-main ${isMotionEdge ? 'bpqa-edge-main--runtime' : ''}`}
          d={path}
          fill="none"
          stroke={isMotionEdge ? motionTrackColor : accent}
          strokeWidth={BPQA_STYLE.edge.width}
          markerEnd={`url(#${markerId})`}
        />
      ) : null}
      {isMotionEdge
        ? Array.from({ length: motionFrequency }, (_, index) => (
            <MotionShape
              key={`${safeId}-shape-${index}`}
              id={`${safeId}-shape-${index}`}
              index={index}
              color={motionColor}
              animateMotionProps={{
                begin: `${(-motionSpeedSeconds / motionFrequency) * index}s`,
                dur: `${motionSpeedSeconds}s`,
                repeatCount: 'indefinite',
                path,
              }}
            />
          ))
        : (
            showTrack ? (
              <path
                className="bpqa-edge-flow"
                d={path}
                fill="none"
                stroke={motionTrackColor}
                strokeWidth={BPQA_STYLE.edge.flowWidth}
                strokeDasharray={`${BPQA_STYLE.edge.dashLength} ${BPQA_STYLE.edge.dashGap}`}
              />
            ) : null
          )}
      {label ? (
        <EdgeLabelRenderer>
          <div
            className="bpqa-edge-label"
            style={{
              transform: `translate(-50%, -50%) translate(${labelX}px, ${labelY}px)`,
            }}
          >
            {label}
          </div>
        </EdgeLabelRenderer>
      ) : null}
    </>
  );
}

function makeCustomPathShape(pathD: string): AnimatedSvg {
  return ({ animateMotionProps, color }: AnimatedSvgProps) => (
    <path className="bpqa-edge-orb" d={pathD} fill={color}>
      <animateMotion {...animateMotionProps} />
    </path>
  );
}

function getAnimatedSvgShape(shapeName: string | undefined, customShapePath?: string): AnimatedSvg {
  // A "custom" shape carries its SVG path straight from the host/graph data so
  // users can supply arbitrary marker geometry without registering code.
  if (shapeName === 'custom' && customShapePath && customShapePath.trim()) {
    return makeCustomPathShape(customShapePath.trim());
  }
  return animatedSvgShapes[shapeName ?? ''] ?? shapes.orb;
}

/** Names of every shape currently available for the animated motion markers. */
export function getRegisteredAnimatedSvgShapeNames(): string[] {
  return Object.keys(animatedSvgShapes);
}

function getRoutedStepPath(args: {
  sourceX: number;
  sourceY: number;
  sourcePosition?: Position;
  targetX: number;
  targetY: number;
  targetPosition?: Position;
  routeOffset: number;
  routeLane?: number;
}): [string, number, number] {
  const isVertical =
    args.sourcePosition === Position.Bottom ||
    args.sourcePosition === Position.Top ||
    args.targetPosition === Position.Bottom ||
    args.targetPosition === Position.Top;

  if (isVertical) {
    if (Math.abs(args.routeOffset) > 0) {
      const direction = Math.sign(args.routeOffset);
      const sideMargin = Math.max(Math.abs(args.routeOffset), MIN_SIDE_ROUTE_OFFSET);
      const roadX =
        direction > 0
          ? Math.max(args.sourceX, args.targetX) + sideMargin
          : Math.min(args.sourceX, args.targetX) - sideMargin;
      const sourceStubY = args.sourceY + getVerticalStubDirection(args.sourcePosition, 1) * VERTICAL_ENDPOINT_STUB;
      const targetStubY = args.targetY + getVerticalStubDirection(args.targetPosition, -1) * VERTICAL_ENDPOINT_STUB;

      return [
        `M${args.sourceX},${args.sourceY} L${args.sourceX},${sourceStubY} L${roadX},${sourceStubY} L${roadX},${targetStubY} L${args.targetX},${targetStubY} L${args.targetX},${args.targetY}`,
        roadX,
        (sourceStubY + targetStubY) / 2,
      ];
    }

    const midY = (args.sourceY + args.targetY) / 2;
    return [
      `M${args.sourceX},${args.sourceY} L${args.sourceX},${midY} L${args.targetX},${midY} L${args.targetX},${args.targetY}`,
      (args.sourceX + args.targetX) / 2,
      midY,
    ];
  }

  const midX = (args.sourceX + args.targetX) / 2 + args.routeOffset;
  return [
    `M${args.sourceX},${args.sourceY} L${midX},${args.sourceY} L${midX},${args.targetY} L${args.targetX},${args.targetY}`,
    midX,
    (args.sourceY + args.targetY) / 2,
  ];
}

function getVerticalStubDirection(position: Position | undefined, fallback: 1 | -1): 1 | -1 {
  if (position === Position.Top) {
    return -1;
  }
  if (position === Position.Bottom) {
    return 1;
  }
  return fallback;
}
