import { useCallback, useRef } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnPrefetchChange } from '../ShadowListViewNativeComponent';
import type { PrefetchDataSource } from '../types';

interface UsePrefetchOptions {
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
 * Rows ahead of and behind the mounted ones, for the app to load early. Native tracks them by
 * key with the same rules as the native lists, see host/Prefetch.hpp, and sends what to
 * prefetch or cancel whenever the mounted rows or the data change.
 */
export function usePrefetch({
  prefetchDataSource,
  prefetchRows,
}: UsePrefetchOptions): UsePrefetchResult {
  const prefetchDataSourceRef = useRef(prefetchDataSource);
  prefetchDataSourceRef.current = prefetchDataSource;
  const handlePrefetchChange = useCallback(
    (event: { nativeEvent: OnPrefetchChange }) => {
      const { prefetch, cancel } = event.nativeEvent;
      const source = prefetchDataSourceRef.current;
      if (cancel.length > 0) source?.cancelPrefetchingForItems?.(cancel);
      if (prefetch.length > 0) source?.prefetchItems(prefetch);
    },
    []
  );
  const enabled = prefetchDataSource !== undefined && prefetchRows > 0;
  return {
    nativePrefetchRows: enabled ? Math.floor(prefetchRows) : 0,
    handlePrefetchChange: enabled ? handlePrefetchChange : undefined,
  };
}
