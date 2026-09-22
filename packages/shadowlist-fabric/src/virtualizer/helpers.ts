// Default extra rows mounted on each side of the screen. See overscanRows for how to pick one.
export const SHADOWLIST_OVERSCAN = 4;

/*
 * Default extra rows mounted ahead in the scroll direction, overridden by overscanRowsLeading.
 * A blank cell in a fling is a row native reached before React mounted it, so rows ahead are
 * what help. Rows behind only cost render work. A fling mounts this many ahead and
 * SHADOWLIST_OVERSCAN behind, and a list at rest keeps SHADOWLIST_OVERSCAN on both sides.
 */
export const SHADOWLIST_OVERSCAN_LEADING = 10;

// Maps snapToAlignment to the native enum value.
export const SNAP_ALIGNMENT = { start: 0, center: 1, end: 2 } as const;

/*
 * Device trace. Debug Apple builds started with SHADOWLIST_FRAME_TRACE=1 install
 * __shadowlistTrace, see ShadowListTrace.h. It prints [SLJ] lines on the same clock as the
 * native [SLF] frame trace. Elsewhere it's missing and a call costs one global read, so
 * only build messages behind slTraceEnabled().
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

/*
 * Milliseconds on a steady clock, for trace durations.
 */
export function slTraceNow(): number {
  return (
    (globalThis as { performance?: { now(): number } }).performance?.now() ??
    Date.now()
  );
}

// Rows rendered since the last commit trace line. All lists share this counter.
let rowRenderCount = 0;

export function countRowRender(): void {
  if (slTraceEnabled()) rowRenderCount++;
}

export function takeRowRenderCount(): number {
  const count = rowRenderCount;
  rowRenderCount = 0;
  return count;
}

/*
 * The native view's tag, which is the id= in its [SLF] lines, or -1 before it mounts.
 */
export function nativeTagOf(instance: unknown): number {
  return (instance as { __nativeTag?: number } | null)?.__nativeTag ?? -1;
}

/*
 * How next differs from previous at the edges. pre counts rows added before the old first
 * row, app after the old last row, and mid in between, negative for removals. replace means
 * an edge row is gone.
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

/*
 * Move one element, returning a new array.
 */
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
