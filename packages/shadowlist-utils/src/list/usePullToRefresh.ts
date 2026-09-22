import { useCallback, useMemo, useRef, useState } from 'react';

export interface UsePullToRefreshOptions {
  // Gets any error from refresh. Without it the rejection goes unhandled.
  onError?: (error: unknown) => void;
}

export interface PullToRefresh {
  refreshing: boolean;
  onRefresh: () => void;
}

/**
 * Drives a pull to refresh control from any async refresh, such as a query's `refetch`.
 *
 * Use this instead of a query's `isRefetching`. That flag is also true for background
 * refetches, which would show the spinner when the reader never pulled.
 *
 * `refresh` and `onError` are read at call time, so they do not need to be stable.
 *
 * @example
 * const poll = useQuery(pollQuery);
 * const { refreshing, onRefresh } = usePullToRefresh(poll.refetch);
 * <ShadowList refreshing={refreshing} onRefresh={onRefresh} ... />
 */
export function usePullToRefresh(
  refresh: () => Promise<unknown>,
  options: UsePullToRefreshOptions = {}
): PullToRefresh {
  const refreshRef = useRef(refresh);
  refreshRef.current = refresh;
  const optionsRef = useRef(options);
  optionsRef.current = options;

  // A guard that flips right away. State changes on the next render, so a second pull could slip past.
  const inFlightRef = useRef(false);
  const [refreshing, setRefreshing] = useState(false);

  const onRefresh = useCallback(() => {
    if (inFlightRef.current) return;
    inFlightRef.current = true;
    setRefreshing(true);
    // Run inside then, so a synchronous throw still reaches finally.
    const pending = Promise.resolve()
      .then(() => refreshRef.current())
      .finally(() => {
        inFlightRef.current = false;
        setRefreshing(false);
      });
    const { onError } = optionsRef.current;
    if (onError) pending.catch(onError);
  }, []);

  return useMemo(() => ({ refreshing, onRefresh }), [refreshing, onRefresh]);
}
