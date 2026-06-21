import type { BPQAGraphPayload } from './types';

export interface BPQAHostHandlers {
  onGraph: (graph: BPQAGraphPayload) => void;
  onSelected: (nodeId: string) => void;
  onLoading: (message: string, progress: number) => void;
}

export interface BPQAHostBridgeOptions {
  notify?: (path: string) => void;
  reportErrors?: boolean;
}

declare global {
  interface Window {
    BPQASetGraph?: (graph: BPQAGraphPayload | string) => void;
    BPQASetSelected?: (nodeId: string) => void;
    BPQASetLoading?: (message: string, progress: number) => void;
    BPQAClearLoading?: () => void;
    /** Host-callable notifier so the C++ readiness probe never navigates the main frame. */
    BPQANotifyHost?: (path: string) => void;
  }
}

// Benign browser warnings that surface as window 'error' events but are not real
// failures. Escalating these to the host would otherwise flip the view into an
// error state (and historically fed the reload loop).
function isBenignError(message: string): boolean {
  return (
    message.includes('ResizeObserver loop limit exceeded') ||
    message.includes('ResizeObserver loop completed with undelivered notifications')
  );
}

export function encodeHostPayload(value: string): string {
  const bytes = new TextEncoder().encode(value);
  let binary = '';
  bytes.forEach((byte) => {
    binary += String.fromCharCode(byte);
  });

  return btoa(binary).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/g, '');
}

export function makeHostUrl(path: string): string {
  return `bpqa://${path}`;
}

// Host notifications are delivered to the C++ side by navigating to a custom
// `bpqa://` URL, which it intercepts in OnBeforeNavigation. We MUST NOT navigate
// the main frame to do this: in UE's CEF build, assigning the main document's
// `location.href` to the custom scheme reloads the page (load start -> ready ->
// graph push -> render -> notify -> reload ...), an endless refresh loop. A
// short-lived hidden iframe carries the navigation instead, so the host still
// receives every message while the main document never reloads.
export function notifyHost(path: string): void {
  const url = makeHostUrl(path);
  try {
    const frame = document.createElement('iframe');
    frame.style.display = 'none';
    frame.setAttribute('aria-hidden', 'true');
    frame.src = url;
    const parent = document.body ?? document.documentElement;
    parent.appendChild(frame);
    window.setTimeout(() => frame.remove(), 0);
  } catch {
    window.dispatchEvent(new CustomEvent('bpqa-host-message', { detail: url }));
  }
}

export function notifyHostError(message: string, notify: (path: string) => void = notifyHost): void {
  notify(`error/${encodeHostPayload(message)}`);
}

export function notifyHostRendered(nodeCount: number, notify: (path: string) => void = notifyHost): void {
  notify(`rendered/${Math.max(0, Math.trunc(nodeCount))}`);
}

export function notifyHostSelect(nodeId: string, notify: (path: string) => void = notifyHost): void {
  notify(`select/${encodeHostPayload(nodeId)}`);
}

export function notifyHostOpen(nodeId: string, notify: (path: string) => void = notifyHost): void {
  notify(`open/${encodeHostPayload(nodeId)}`);
}

// Max characters of base64 payload carried per bridge navigation. Kept well under
// the browser URL length ceiling so large PNGs stay inside the iframe channel.
const IMAGE_CHUNK_SIZE = 16000;

function toUrlSafeBase64(standard: string): string {
  return standard.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/g, '');
}

// Streams a rendered PNG to the C++ host through the `bpqa://image/*` bridge. The
// editor's offline CEF build has no download handler, so the host (which already
// owns JSON/Mermaid/DOT exports) writes the file to disk instead. The data is
// url-safe base64, indexed per chunk so the host can reassemble it in order
// regardless of navigation delivery order.
export function sendImageToHost(
  filename: string,
  pngBase64: string,
  notify: (path: string) => void = notifyHost,
): void {
  const safe = toUrlSafeBase64(pngBase64);
  notify(`image/begin/${encodeHostPayload(filename)}`);
  let index = 0;
  for (let offset = 0; offset < safe.length; offset += IMAGE_CHUNK_SIZE) {
    notify(`image/chunk/${index}/${safe.slice(offset, offset + IMAGE_CHUNK_SIZE)}`);
    index += 1;
  }
  notify(`image/end/${index}`);
}

export function createHostBridge(handlers: BPQAHostHandlers, options: BPQAHostBridgeOptions = {}): () => void {
  const notify = options.notify ?? notifyHost;
  const reportErrors = options.reportErrors ?? true;

  const report = (context: string, error: unknown) => {
    if (!reportErrors) {
      return;
    }

    const message = error instanceof Error ? error.message : String(error);
    notifyHostError(`${context}: ${message}`, notify);
  };

  window.BPQASetGraph = (payload: BPQAGraphPayload | string) => {
    try {
      const graph = typeof payload === 'string' ? (JSON.parse(payload) as BPQAGraphPayload) : payload;
      handlers.onGraph(graph);
    } catch (error) {
      report('BPQASetGraph failed', error);
    }
  };

  window.BPQASetSelected = (nodeId: string) => {
    try {
      handlers.onSelected(String(nodeId ?? ''));
    } catch (error) {
      report('BPQASetSelected failed', error);
    }
  };

  window.BPQASetLoading = (message: string, progress: number) => {
    try {
      const safeProgress = Number.isFinite(progress) ? Math.min(1, Math.max(0, progress)) : 0;
      handlers.onLoading(String(message ?? ''), safeProgress);
    } catch (error) {
      report('BPQASetLoading failed', error);
    }
  };

  window.BPQAClearLoading = () => {
    try {
      handlers.onLoading('', 0);
    } catch (error) {
      report('BPQAClearLoading failed', error);
    }
  };

  // Lets the host's readiness probe deliver `bpqa://ready` through the iframe
  // bridge instead of navigating the main frame.
  window.BPQANotifyHost = (path: string) => {
    try {
      notify(String(path ?? ''));
    } catch (error) {
      report('BPQANotifyHost failed', error);
    }
  };

  const errorListener = (event: ErrorEvent) => {
    const message = event.error instanceof Error ? event.error.message : String(event.message ?? '');
    if (isBenignError(message)) {
      return;
    }
    report('JavaScript error', event.error ?? event.message);
  };
  const rejectionListener = (event: PromiseRejectionEvent) => {
    report('Unhandled promise rejection', event.reason);
  };

  window.addEventListener('error', errorListener);
  window.addEventListener('unhandledrejection', rejectionListener);
  window.setTimeout(() => notify('ready'), 0);

  return () => {
    delete window.BPQASetGraph;
    delete window.BPQASetSelected;
    delete window.BPQASetLoading;
    delete window.BPQAClearLoading;
    delete window.BPQANotifyHost;
    window.removeEventListener('error', errorListener);
    window.removeEventListener('unhandledrejection', rejectionListener);
  };
}
