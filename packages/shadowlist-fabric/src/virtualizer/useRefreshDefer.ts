import { useCallback, useEffect, useRef, useState } from 'react';

interface UseRefreshDeferOptions<ElementT> {
  data: ReadonlyArray<ElementT>;
  refreshing: boolean;
  onRefresh: (() => void) | undefined;
  inverted: boolean;
  horizontal: boolean;
}

interface UseRefreshDeferResult<ElementT> {
  data: ReadonlyArray<ElementT>;
  handleRefreshSettle: () => void;
}

/*
 * Refresh-prepend deferral: hold data changes that arrive mid-refresh until the
 * spinner has fully retracted (onRefreshSettle), then apply them as an ordinary
 * prepend so MVCP anchors them. Non-refresh changes pass through. Only active for
 * vertical, non-inverted lists with an onRefresh handler.
 */
export function useRefreshDefer<ElementT>({
  data: dataProp,
  refreshing,
  onRefresh,
  inverted,
  horizontal,
}: UseRefreshDeferOptions<ElementT>): UseRefreshDeferResult<ElementT> {
  const refreshDeferEnabled = !!onRefresh && !inverted && !horizontal;

  const [committedData, setCommittedData] =
    useState<ReadonlyArray<ElementT>>(dataProp);
  const refreshHoldingRef = useRef(false);
  const refreshHeldDataRef = useRef<ReadonlyArray<ElementT> | null>(null);
  const prevRefreshingRef = useRef(refreshing);

  if (refreshDeferEnabled && !prevRefreshingRef.current && refreshing) {
    // A refresh just started: hold subsequent data changes until it settles.
    refreshHoldingRef.current = true;
  }
  prevRefreshingRef.current = refreshing;

  if (dataProp !== committedData) {
    if (refreshHoldingRef.current) {
      refreshHeldDataRef.current = dataProp;
    } else {
      setCommittedData(dataProp);
    }
  }

  // Apply the held refresh-prepend once native reports the spinner has fully retracted.
  const handleRefreshSettle = useCallback(() => {
    refreshHoldingRef.current = false;
    if (refreshHeldDataRef.current !== null) {
      setCommittedData(refreshHeldDataRef.current);
      refreshHeldDataRef.current = null;
    }
  }, []);

  /*
   * Safety net: release the held prepend shortly after refresh ends in case onRefreshSettle
   * never arrives (e.g. a platform that doesn't emit it). On iOS it fires first, so this is
   * a no-op there.
   */
  const prevRefreshingForFallbackRef = useRef(refreshing);
  useEffect(() => {
    const wasRefreshing = prevRefreshingForFallbackRef.current;
    prevRefreshingForFallbackRef.current = refreshing;
    if (refreshDeferEnabled && wasRefreshing && !refreshing) {
      const timer = setTimeout(handleRefreshSettle, 1200);
      return () => clearTimeout(timer);
    }
    return undefined;
  }, [refreshing, refreshDeferEnabled, handleRefreshSettle]);

  return { data: committedData, handleRefreshSettle };
}
