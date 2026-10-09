import { useLayoutEffect, useRef, type ComponentRef } from 'react';
import type ShadowListView from '../ShadowListViewNativeComponent';
import {
  describeDataChange,
  nativeTagOf,
  slTrace,
  slTraceEnabled,
  slTraceNow,
  takeRowRenderCount,
} from './helpers';

interface UseRenderTraceOptions<ElementT> {
  startRef: { current: number };
  viewRef: { current: ComponentRef<typeof ShadowListView> | null };
  data: ReadonlyArray<ElementT>;
  renderIndices: ReadonlyArray<number>;
  refreshing: boolean;
  keyExtractor: (element: ElementT, index: number) => string;
}

/*
 * Device trace only. When the list's render started, for the jsms= of its commit line. Called
 * first in the list's render.
 */
export function useRenderTraceStart(): { current: number } {
  const startRef = useRef(0);
  if (slTraceEnabled()) {
    startRef.current = slTraceNow();
  }
  return startRef;
}

/*
 * Device trace only. One render line per commit with the mounted rows, the rows rendered
 * since the last line, the render time and how data changed.
 */
export function useRenderTrace<ElementT>({
  startRef,
  viewRef,
  data,
  renderIndices,
  refreshing,
  keyExtractor,
}: UseRenderTraceOptions<ElementT>): void {
  const traceDataRef = useRef<ReadonlyArray<ElementT> | null>(null);
  useLayoutEffect(() => {
    if (!slTraceEnabled()) return;
    const previousData = traceDataRef.current;
    traceDataRef.current = data;
    const first = renderIndices[0] ?? -1;
    const last = renderIndices[renderIndices.length - 1] ?? -1;
    const elapsed = slTraceNow() - startRef.current;
    slTrace(
      `render id=${nativeTagOf(viewRef.current)} n=${data.length}` +
        ` mounted=${first}..${last} rows=${takeRowRenderCount()}` +
        ` jsms=${elapsed.toFixed(1)} refreshing=${refreshing ? 1 : 0}` +
        (previousData !== data
          ? ` data=${describeDataChange(previousData, data, keyExtractor)}`
          : '')
    );
  });
}

/*
 * Device trace only. Logs which of the named inputs got a new identity since the last
 * render. An input that changes every commit can rebuild every mounted row. Check this
 * first when a list re-renders more rows than changed.
 */
export function useInputTrace(
  label: string,
  names: ReadonlyArray<string>,
  inputs: ReadonlyArray<unknown>
): void {
  const previousRef = useRef<ReadonlyArray<unknown>>([]);
  if (!slTraceEnabled()) return;
  const changed = names.filter(
    (_name, index) => previousRef.current[index] !== inputs[index]
  );
  previousRef.current = inputs;
  if (changed.length > 0) {
    slTrace(`${label} changed=${changed.join(',')}`);
  }
}
