import { useCallback, useRef } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnPrefetchChange } from '../ShadowListViewNativeComponent';
import type { PrefetchDataSource } from '../types';
import type { RowSource } from './rowSource';

interface UsePrefetchOptions {
  source: RowSource<unknown>;
  prefetchDataSource: PrefetchDataSource | undefined;
  prefetchRows: number;
}

interface UsePrefetchResult {
  /*
   * Rows on each side of the mounted ones native tracks, 0 for none.
   */
  nativePrefetchRows: number;
  handlePrefetchChange:
    CodegenTypes.DirectEventHandler<OnPrefetchChange, never> | undefined;
}

/*
 * The indices of keys in the data rendered now, low to high, without the keys that left it.
 * indices hold where each key was in the keys native had, the first place to look.
 */
function currentIndices(
  source: RowSource<unknown>,
  keys: ReadonlyArray<string>,
  indices: ReadonlyArray<number>
): number[] {
  const current: number[] = [];
  for (let position = 0; position < keys.length; position++) {
    const index = source.indexOfKey(keys[position]!, indices[position]);
    if (index >= 0) current.push(index);
  }
  return current.sort((a, b) => a - b);
}

/*
 * Rows ahead of and behind the mounted ones, for the app to load early. Native tracks them by
 * key with the same rules as the native lists, see host/Prefetch.hpp, and sends what to
 * prefetch or cancel whenever the mounted rows or the data change. Each row comes with its key.
 * The app hears indices in the data it renders now, even when that changed after native sent
 * them.
 */
export function usePrefetch({
  source,
  prefetchDataSource,
  prefetchRows,
}: UsePrefetchOptions): UsePrefetchResult {
  const prefetchDataSourceRef = useRef(prefetchDataSource);
  prefetchDataSourceRef.current = prefetchDataSource;
  const sourceRef = useRef(source);
  sourceRef.current = source;
  const handlePrefetchChange = useCallback(
    (event: { nativeEvent: OnPrefetchChange }) => {
      const { prefetch, cancel, prefetchKeys, cancelKeys } = event.nativeEvent;
      const dataSource = prefetchDataSourceRef.current;
      const rows = sourceRef.current;
      const cancelIndices = currentIndices(rows, cancelKeys, cancel);
      const prefetchIndices = currentIndices(rows, prefetchKeys, prefetch);
      if (cancelIndices.length > 0) {
        dataSource?.cancelPrefetchingForItems?.(cancelIndices);
      }
      if (prefetchIndices.length > 0)
        dataSource?.prefetchItems(prefetchIndices);
    },
    []
  );
  const enabled = prefetchDataSource !== undefined && prefetchRows > 0;
  return {
    nativePrefetchRows: enabled ? Math.floor(prefetchRows) : 0,
    handlePrefetchChange: enabled ? handlePrefetchChange : undefined,
  };
}
