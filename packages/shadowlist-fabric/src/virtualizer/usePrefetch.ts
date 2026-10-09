import { useEffect, useRef } from 'react';
import type { PrefetchDataSource } from '../types';
import { PrefetchTracker, prefetchWindow } from './prefetch';

interface UsePrefetchOptions {
  keys: ReadonlyArray<string>;
  keyToIndex: ReadonlyMap<string, number>;
  mountedIndices: number[];
  prefetchDataSource: PrefetchDataSource | undefined;
  prefetchRows: number;
}

/*
 * Rows ahead of and behind the mounted ones, for the app to load early.
 */
export function usePrefetch({
  keys,
  keyToIndex,
  mountedIndices,
  prefetchDataSource,
  prefetchRows,
}: UsePrefetchOptions): void {
  const prefetchTrackerRef = useRef<PrefetchTracker | null>(null);
  const prefetchDataSourceRef = useRef(prefetchDataSource);
  prefetchDataSourceRef.current = prefetchDataSource;
  const hasPrefetch = prefetchDataSource !== undefined;
  const mountedLow = mountedIndices[0] ?? -1;
  const mountedHigh = mountedIndices[mountedIndices.length - 1] ?? -1;
  useEffect(() => {
    if (!hasPrefetch) {
      prefetchTrackerRef.current = null;
      return;
    }
    if (prefetchTrackerRef.current === null) {
      prefetchTrackerRef.current = new PrefetchTracker();
    }
    const mounted =
      mountedLow >= 0 ? { low: mountedLow, high: mountedHigh } : null;
    const { prefetch, cancel } = prefetchTrackerRef.current.update(
      keys,
      keyToIndex,
      mounted,
      prefetchWindow(mounted, prefetchRows, keys.length)
    );
    const source = prefetchDataSourceRef.current;
    if (cancel.length > 0) source?.cancelPrefetchingForItems?.(cancel);
    if (prefetch.length > 0) source?.prefetchItems(prefetch);
  }, [hasPrefetch, mountedLow, mountedHigh, keys, keyToIndex, prefetchRows]);
}
