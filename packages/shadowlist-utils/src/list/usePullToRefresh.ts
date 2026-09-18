import { useCallback, useMemo, useRef, useState } from 'react';

export interface UsePullToRefreshOptions {
  // Receives a throw or rejection from `refresh`. Without it the rejection is left unhandled.
  onError?: (error: unknown) => void;
}

export interface PullToRefresh {
  refreshing: boolean;
  onRefresh: () => void;
}

/**
 * Drives a pull-to-refresh control from any async refresh, such as a query's `refetch`.
 *
 * Use this rather than binding `refreshing` to a query's `isRefetching`: that flag is also
 * true for background refetches (app foregrounded, cache invalidated), which would pull
 * the spinner down over the list without the reader asking for it.
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

  // Synchronous guard: state only flips on the next render, a second pull could slip past it.
  const inFlightRef = useRef(false);
  const [refreshing, setRefreshing] = useState(false);

  const onRefresh = useCallback(() => {
    if (inFlightRef.current) return;
    inFlightRef.current = true;
    setRefreshing(true);
    // Called inside `.then` so a synchronous throw still reaches `.finally`.
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
