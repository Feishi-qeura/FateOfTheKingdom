import type { BPQAGraphEdge, BPQAGraphNode, BPQANodeVisual, BPQAViewMode } from './types';

export const BPQA_STYLE = {
  colors: {
    inactiveAccent: '#788396',
    inactiveGlow: 'rgba(120, 131, 150, 0.18)',
    inactiveBackground: '#171b23',
    inactiveForeground: '#b7c0cf',
    folderAccent: '#e0934d',
    folderGlow: 'rgba(224, 147, 77, 0.42)',
    folderBackground: '#241a10',
    blueprintAccent: '#3aa7ff',
    blueprintGlow: 'rgba(58, 167, 255, 0.58)',
    blueprintBackground: '#0c1d32',
    eventAccent: '#ff5c6f',
    eventGlow: 'rgba(255, 92, 111, 0.58)',
    eventBackground: '#2d1018',
    functionAccent: '#5ea7ff',
    functionGlow: 'rgba(94, 167, 255, 0.58)',
    functionBackground: '#0c1d32',
    macroAccent: '#c58cff',
    macroGlow: 'rgba(197, 140, 255, 0.54)',
    macroBackground: '#201434',
    constructionAccent: '#59d39a',
    constructionGlow: 'rgba(89, 211, 154, 0.54)',
    constructionBackground: '#10241f',
    assetBackground: '#0b2424',
    foreground: '#eef6ff',
    selected: '#ffd65a',
    staticEdge: '#38d6bd',
    runtimeEdge: '#63a9ff',
    edgeFlow: '#ffffff',
  },
  edge: {
    width: 2.4,
    glowWidth: 8,
    flowWidth: 2,
    dashLength: 12,
    dashGap: 18,
    animationSeconds: 1.65,
  },
} as const;

export function normalizeViewMode(mode: unknown): BPQAViewMode {
  if (
    mode === 'StaticDependency' ||
    mode === 'FolderDependency' ||
    mode === 'FolderAssetDependency' ||
    mode === 'RuntimeFlow' ||
    mode === 'NodeFlow'
  ) {
    return mode;
  }

  return 'StaticDependency';
}

export function isBlueprintNode(node: BPQAGraphNode): boolean {
  return Boolean(node.isBlueprint) || /blueprint/i.test(node.type ?? '') || /blueprint/i.test(node.className ?? '');
}

export function isUnexecutedRuntimeCandidate(node: BPQAGraphNode): boolean {
  return (node.components ?? []).some((component) => component === 'ExecutionState:Inactive');
}

export function isFolderNode(node: BPQAGraphNode): boolean {
  return node.type === 'Folder' || node.type === 'ExternalFolder' || node.id.startsWith('Folder:');
}

export function getNodeVisual(
  node: BPQAGraphNode,
  connected: boolean,
  viewMode: BPQAViewMode = 'StaticDependency',
): BPQANodeVisual {
  if (isFolderNode(node)) {
    return makeVisual('folder', BPQA_STYLE.colors.folderAccent, BPQA_STYLE.colors.folderGlow, BPQA_STYLE.colors.folderBackground);
  }

  const normalizedViewMode = normalizeViewMode(viewMode);
  const runtimeKind = isRuntimeSemanticNode(node) ? classifyRuntimeNode(node) : undefined;
  const isRuntimeView = normalizedViewMode === 'RuntimeFlow' || normalizedViewMode === 'NodeFlow';

  if (isRuntimeView) {
    if (!connected || isUnexecutedRuntimeCandidate(node)) {
      return makeVisual(
        'inactive',
        BPQA_STYLE.colors.inactiveAccent,
        BPQA_STYLE.colors.inactiveGlow,
        BPQA_STYLE.colors.inactiveBackground,
        BPQA_STYLE.colors.inactiveForeground,
      );
    }

    if (runtimeKind === 'event') {
      return makeVisual('event', BPQA_STYLE.colors.eventAccent, BPQA_STYLE.colors.eventGlow, BPQA_STYLE.colors.eventBackground);
    }
    if (runtimeKind === 'function') {
      return makeVisual(
        'function',
        BPQA_STYLE.colors.functionAccent,
        BPQA_STYLE.colors.functionGlow,
        BPQA_STYLE.colors.functionBackground,
      );
    }
    if (runtimeKind === 'macro') {
      return makeVisual('macro', BPQA_STYLE.colors.macroAccent, BPQA_STYLE.colors.macroGlow, BPQA_STYLE.colors.macroBackground);
    }
    if (runtimeKind === 'construction') {
      return makeVisual(
        'construction',
        BPQA_STYLE.colors.constructionAccent,
        BPQA_STYLE.colors.constructionGlow,
        BPQA_STYLE.colors.constructionBackground,
      );
    }
  }

  if (!connected && isBlueprintNode(node)) {
    return makeVisual(
      'inactive',
      BPQA_STYLE.colors.inactiveAccent,
      BPQA_STYLE.colors.inactiveGlow,
      BPQA_STYLE.colors.inactiveBackground,
      BPQA_STYLE.colors.inactiveForeground,
    );
  }

  if (runtimeKind === 'event') {
    return makeVisual('event', BPQA_STYLE.colors.eventAccent, BPQA_STYLE.colors.eventGlow, BPQA_STYLE.colors.eventBackground);
  }
  if (runtimeKind === 'function') {
    return makeVisual('function', BPQA_STYLE.colors.functionAccent, BPQA_STYLE.colors.functionGlow, BPQA_STYLE.colors.functionBackground);
  }
  if (runtimeKind === 'macro') {
    return makeVisual('macro', BPQA_STYLE.colors.macroAccent, BPQA_STYLE.colors.macroGlow, BPQA_STYLE.colors.macroBackground);
  }
  if (runtimeKind === 'construction') {
    return makeVisual(
      'construction',
      BPQA_STYLE.colors.constructionAccent,
      BPQA_STYLE.colors.constructionGlow,
      BPQA_STYLE.colors.constructionBackground,
    );
  }

  if (isBlueprintNode(node)) {
    return makeVisual(
      'blueprint',
      BPQA_STYLE.colors.blueprintAccent,
      BPQA_STYLE.colors.blueprintGlow,
      BPQA_STYLE.colors.blueprintBackground,
    );
  }

  const assetAccent = getAssetAccent(node);
  return makeVisual('asset', assetAccent, alphaColor(assetAccent, 0.46), BPQA_STYLE.colors.assetBackground);
}

export function getEdgeAccent(edge: BPQAGraphEdge, mode: BPQAViewMode): string {
  if (mode === 'RuntimeFlow' || /runtime|flow|sequence/i.test(edge.type ?? '')) {
    return BPQA_STYLE.colors.runtimeEdge;
  }

  return BPQA_STYLE.colors.staticEdge;
}

function isRuntimeSemanticNode(node: BPQAGraphNode): boolean {
  return Boolean(classifyRuntimeNode(node));
}

function classifyRuntimeNode(node: BPQAGraphNode): 'event' | 'function' | 'macro' | 'construction' | undefined {
  const type = `${node.type ?? ''} ${node.className ?? ''}`.toLowerCase();
  if (type.includes('construction')) {
    return 'construction';
  }
  if (type.includes('macro')) {
    return 'macro';
  }
  if (type.includes('function')) {
    return 'function';
  }
  if (type.includes('event')) {
    return 'event';
  }
  return undefined;
}

function getAssetAccent(node: BPQAGraphNode): string {
  const key = (node.type || node.className || node.assetPath || node.packageName || node.id).trim().toLowerCase();
  if (!key) {
    return '#38d6bd';
  }

  const hash = hashString(key);
  const hue = hash % 360;
  const saturation = 66 + ((hash >> 8) % 18);
  const lightness = 54 + ((hash >> 16) % 10);
  return hslToHex(hue, saturation, lightness);
}

function makeVisual(
  variant: BPQANodeVisual['variant'],
  accent: string,
  glow: string,
  background: string,
  foreground: string = BPQA_STYLE.colors.foreground,
): BPQANodeVisual {
  return {
    variant,
    accent,
    glow,
    background,
    foreground,
    border: accent,
  };
}

function alphaColor(value: string, alpha: number): string {
  const rgba = hexToRgb(value);
  if (!rgba) {
    return `rgba(255, 255, 255, ${alpha})`;
  }

  return `rgba(${rgba.r}, ${rgba.g}, ${rgba.b}, ${alpha})`;
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

function hslToHex(hue: number, saturation: number, lightness: number): string {
  const s = Math.max(0, Math.min(100, saturation)) / 100;
  const l = Math.max(0, Math.min(100, lightness)) / 100;
  const c = (1 - Math.abs(2 * l - 1)) * s;
  const h = ((hue % 360) + 360) % 360 / 60;
  const x = c * (1 - Math.abs((h % 2) - 1));
  let r = 0;
  let g = 0;
  let b = 0;

  if (h >= 0 && h < 1) {
    r = c;
    g = x;
  } else if (h < 2) {
    r = x;
    g = c;
  } else if (h < 3) {
    g = c;
    b = x;
  } else if (h < 4) {
    g = x;
    b = c;
  } else if (h < 5) {
    r = x;
    b = c;
  } else {
    r = c;
    b = x;
  }

  const m = l - c / 2;
  const toHex = (value: number) => Math.round((value + m) * 255).toString(16).padStart(2, '0');
  return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
}

function hashString(value: string): number {
  let hash = 2166136261;
  for (let index = 0; index < value.length; index += 1) {
    hash ^= value.charCodeAt(index);
    hash = Math.imul(hash, 16777619);
  }

  return hash >>> 0;
}
