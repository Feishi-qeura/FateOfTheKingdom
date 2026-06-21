import { describe, expect, it } from 'vitest';
import { getNodeVisual } from './styleRules';

describe('styleRules', () => {
  it('assigns different accents to different static asset classes', () => {
    const material = getNodeVisual({ id: 'A', type: 'Material' }, true);
    const texture = getNodeVisual({ id: 'B', type: 'Texture' }, true);

    expect(material.variant).toBe('asset');
    expect(texture.variant).toBe('asset');
    expect(material.accent).not.toBe(texture.accent);
  });

  it('distinguishes runtime events and functions with different accents', () => {
    const eventNode = getNodeVisual({ id: 'Event', type: 'BlueprintEvent', isBlueprint: true }, true, 'RuntimeFlow');
    const functionNode = getNodeVisual({ id: 'Function', type: 'BlueprintFunction', isBlueprint: true }, true, 'RuntimeFlow');

    expect(eventNode.accent).not.toBe(functionNode.accent);
    expect(eventNode.variant).not.toBe('inactive');
    expect(functionNode.variant).not.toBe('inactive');
  });
});
