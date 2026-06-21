import { afterEach, describe, expect, it, vi } from 'vitest';
import {
  createHostBridge,
  encodeHostPayload,
  makeHostUrl,
  notifyHostOpen,
  notifyHostRendered,
  notifyHostSelect,
  sendImageToHost,
} from './hostBridge';

afterEach(() => {
  delete window.BPQASetGraph;
  delete window.BPQASetSelected;
  delete window.BPQASetLoading;
  delete window.BPQAClearLoading;
  vi.useRealTimers();
});

describe('host bridge', () => {
  it('installs ready, graph, selection and loading APIs', () => {
    vi.useFakeTimers();
    const notify = vi.fn();
    const onGraph = vi.fn();
    const onSelected = vi.fn();
    const onLoading = vi.fn();
    const cleanup = createHostBridge({ onGraph, onSelected, onLoading }, { notify });

    vi.runOnlyPendingTimers();
    expect(notify).toHaveBeenCalledWith('ready');

    window.BPQASetGraph?.('{"title":"T","nodes":[],"edges":[]}');
    window.BPQASetSelected?.('/Game/A');
    window.BPQASetLoading?.('Loading', 1.5);
    window.BPQAClearLoading?.();

    expect(onGraph).toHaveBeenCalledWith({ title: 'T', nodes: [], edges: [] });
    expect(onSelected).toHaveBeenCalledWith('/Game/A');
    expect(onLoading).toHaveBeenCalledWith('Loading', 1);
    expect(onLoading).toHaveBeenCalledWith('', 0);

    cleanup();
    expect(window.BPQASetGraph).toBeUndefined();
    expect(window.BPQAClearLoading).toBeUndefined();
  });

  it('uses URL-safe Base64 payloads for host commands', () => {
    const notify = vi.fn();
    notifyHostSelect('/Game/蓝图', notify);
    notifyHostOpen('/Game/蓝图', notify);
    notifyHostRendered(4, notify);

    expect(notify).toHaveBeenNthCalledWith(1, `select/${encodeHostPayload('/Game/蓝图')}`);
    expect(notify).toHaveBeenNthCalledWith(2, `open/${encodeHostPayload('/Game/蓝图')}`);
    expect(notify).toHaveBeenNthCalledWith(3, 'rendered/4');
    expect(makeHostUrl('ready')).toBe('bpqa://ready');
  });

  it('streams an image to the host as url-safe indexed chunks', () => {
    const notify = vi.fn();
    // 40000 chars of standard base64 (with + / =) -> 3 chunks at 16000 each.
    const payload = `${'A'.repeat(39997)}+/=`;
    sendImageToHost('运行流程视图.png', payload, notify);

    const calls = notify.mock.calls.map((call) => call[0] as string);
    expect(calls[0]).toBe(`image/begin/${encodeHostPayload('运行流程视图.png')}`);
    expect(calls.filter((path) => path.startsWith('image/chunk/'))).toHaveLength(3);
    expect(calls[calls.length - 1]).toBe('image/end/3');

    // url-safe: no '+', '/', or '=' survive in any chunk.
    const reassembled = calls
      .filter((path) => path.startsWith('image/chunk/'))
      .map((path) => path.replace(/^image\/chunk\/\d+\//, ''))
      .join('');
    expect(reassembled).not.toMatch(/[+/=]/);
    expect(reassembled).toBe(`${'A'.repeat(39997)}-_`);
  });
});
