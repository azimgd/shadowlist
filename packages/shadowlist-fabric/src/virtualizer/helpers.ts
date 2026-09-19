// Number of extra rows mounted on each side of the visible window
export const SHADOWLIST_OVERSCAN = 4;

/*
 * Extra rows mounted ahead of the visible window, in the direction the list is actually
 * travelling. A blank cell during a fling is a row the native side has already scrolled
 * to but React has not mounted yet, so what protects against it is runway in front of the
 * user -- not a bigger buffer behind them, which only costs render work. This is the
 * leading side's total, so a fling mounts SHADOWLIST_OVERSCAN_LEADING rows ahead and
 * SHADOWLIST_OVERSCAN behind; a resting list keeps SHADOWLIST_OVERSCAN on both sides.
 */
export const SHADOWLIST_OVERSCAN_LEADING = 10;

// Maps the public snapToAlignment prop to the native enum value
export const SNAP_ALIGNMENT = { start: 0, center: 1, end: 2 } as const;

/*
 * Device trace. Debug Apple builds launched with SHADOWLIST_FRAME_TRACE=1 install
 * `__shadowlistTrace` natively (see ShadowListTrace.h); it prints `[SLJ]` lines on the
 * same clock as the host's `[SLF]` frame trace. Absent everywhere else, so each call costs
 * one global property read; build messages only behind slTraceEnabled().
 */
interface TraceGlobal {
  __shadowlistTrace?: (message: string) => void;
}

export function slTraceEnabled(): boolean {
  return typeof (globalThis as TraceGlobal).__shadowlistTrace === 'function';
}

export function slTrace(message: string): void {
  (globalThis as TraceGlobal).__shadowlistTrace?.(message);
}

// Milliseconds on a monotonic clock, for trace durations.
export function slTraceNow(): number {
  return (
    (globalThis as { performance?: { now(): number } }).performance?.now() ??
    Date.now()
  );
}

// Rows rendered since the last list commit trace line (all lists share the counter).
let rowRenderCount = 0;

export function countRowRender(): void {
  if (slTraceEnabled()) rowRenderCount++;
}

export function takeRowRenderCount(): number {
  const count = rowRenderCount;
  rowRenderCount = 0;
  return count;
}

// The host view's react tag, the `id=` of its [SLF] lines; -1 before it mounts.
export function nativeTagOf(instance: unknown): number {
  return (instance as { __nativeTag?: number } | null)?.__nativeTag ?? -1;
}

/*
 * How `next` differs from `previous` at the edges: rows added before the old first row
 * (pre), after the old last row (app), and in between (mid, negative for removals).
 * `replace` when either edge row is gone.
 */
export function describeDataChange<ElementT>(
  previous: ReadonlyArray<ElementT> | null,
  next: ReadonlyArray<ElementT>,
  keyExtractor: (element: ElementT, index: number) => string
): string {
  if (previous === null) return `init:${next.length}`;
  if (previous.length === 0 || next.length === 0) {
    return `replace:${previous.length}->${next.length}`;
  }
  const firstKey = keyExtractor(previous[0]!, 0);
  const lastKey = keyExtractor(
    previous[previous.length - 1]!,
    previous.length - 1
  );
  let firstIndex = -1;
  let lastIndex = -1;
  for (let index = 0; index < next.length; index++) {
    const key = keyExtractor(next[index]!, index);
    if (firstIndex < 0 && key === firstKey) firstIndex = index;
    if (key === lastKey) lastIndex = index;
  }
  if (firstIndex < 0 || lastIndex < 0) {
    return `replace:${previous.length}->${next.length}`;
  }
  const middle = lastIndex - firstIndex + 1 - previous.length;
  return `pre:${firstIndex} app:${next.length - 1 - lastIndex}${middle !== 0 ? ` mid:${middle}` : ''}`;
}

// Move the element at `from` to `to`, returning a new array.
export function arrayMove<ElementT>(
  input: ReadonlyArray<ElementT>,
  from: number,
  to: number
): ElementT[] {
  const next = input.slice();
  if (
    from < 0 ||
    from >= next.length ||
    to < 0 ||
    to >= next.length ||
    from === to
  ) {
    return next;
  }
  const moved = next.splice(from, 1)[0] as ElementT;
  next.splice(to, 0, moved);
  return next;
}
