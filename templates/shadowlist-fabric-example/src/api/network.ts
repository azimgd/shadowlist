/*
 * The fake network every endpoint in src/api goes through. A response is built when it arrives,
 * not when it is requested. A slow read sees a write that landed meanwhile, like a real server.
 */
export const network = {
  // Round-trip time, drawn uniformly from this range. Set both to 0 for benchmark runs.
  latencyMs: { min: 250, max: 700 },
  // Share of chat sends that fail, from 0 to 1. Zero by default so scripted traces stay deterministic.
  sendFailureRate: 0,
  // Logs "[SLNET] start" and "[SLNET] end" around every request for scripted recordings that wait for loads.
  log: false,
};

export class RequestFailedError extends Error {
  constructor() {
    super('The request failed.');
    this.name = 'RequestFailedError';
  }
}

export interface RequestSignal {
  readonly aborted: boolean;
  addEventListener(type: 'abort', listener: () => void): void;
  removeEventListener(type: 'abort', listener: () => void): void;
}

export class RequestAbortedError extends Error {
  constructor() {
    super('The request was aborted.');
    this.name = 'AbortError';
  }
}

export function request<T>(
  respond: () => T,
  signal?: RequestSignal
): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    if (signal?.aborted) {
      reject(new RequestAbortedError());
      return;
    }
    const { min, max } = network.latencyMs;
    if (network.log) console.log('[SLNET] start');
    const onAbort = () => {
      clearTimeout(timer);
      if (network.log) console.log('[SLNET] end');
      reject(new RequestAbortedError());
    };
    const timer = setTimeout(
      () => {
        signal?.removeEventListener('abort', onAbort);
        if (network.log) console.log('[SLNET] end');
        try {
          resolve(respond());
        } catch (error) {
          reject(error);
        }
      },
      min + Math.random() * (max - min)
    );
    signal?.addEventListener('abort', onAbort);
  });
}
