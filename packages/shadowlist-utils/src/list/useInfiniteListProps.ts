import { useCallback, useMemo, useRef } from 'react';
import type { InfiniteItem, InfinitePages, ItemsPage } from './InfinitePages';
import { flattenInfiniteItems } from './infiniteItems';
import { usePullToRefresh } from './usePullToRefresh';

export interface InfiniteListQuery<DataT> {
  data: DataT | undefined;
  hasNextPage: boolean;
  hasPreviousPage: boolean;
  isFetching: boolean;
  fetchNextPage: () => Promise<unknown>;
  fetchPreviousPage: () => Promise<unknown>;
  refetch: () => Promise<unknown>;
}

export interface UseInfiniteListPropsOptions {
  refresh?: () => Promise<unknown>;
  /*
   * Receives a throw or rejection from a page fetch or the refresh. React Query's own
   * fetchers resolve on failure (the error lands on the query), so this matters for custom
   * fetchers; without it the rejection is left unhandled.
   */
  onError?: (error: unknown) => void;
}

export interface InfiniteListProps<ItemT> {
  data: ReadonlyArray<ItemT>;
  refreshing: boolean;
  onRefresh: () => void;
  onEndReached: () => void;
  onStartReached: () => void;
}

/**
 * Connects an infinite query to a shadowlist list.
 *
 * - `data` is the flattened rows, memoized on the cache value, so the list only sees a new
 *   array when a page actually changed.
 * - `onEndReached` / `onStartReached` are stable and safe to fire repeatedly: they do
 *   nothing without a page to load, while any fetch is running (a page fetch racing a
 *   refetch would build on stale pages), or while their own fetch is still pending.
 * - `refreshing` reflects pull-to-refresh only, never background refetches.
 *
 * @example
 * const feed = useInfiniteQuery(feedQuery);
 * const list = useInfiniteListProps(feed);
 * if (feed.data === undefined) return <Loading />;
 * return (
 *   <ShadowList
 *     data={list.data}
 *     onEndReached={list.onEndReached}
 *     refreshing={list.refreshing}
 *     onRefresh={list.onRefresh}
 *     renderElement={renderPost}
 *   />
 * );
 */
export function useInfiniteListProps<
  DataT extends InfinitePages<ItemsPage<unknown>>,
>(
  query: InfiniteListQuery<DataT>,
  options: UseInfiniteListPropsOptions = {}
): InfiniteListProps<InfiniteItem<DataT>> {
  const queryRef = useRef(query);
  queryRef.current = query;
  const optionsRef = useRef(options);
  optionsRef.current = options;

  const data = useMemo(() => flattenInfiniteItems(query.data), [query.data]);

  const { refreshing, onRefresh } = usePullToRefresh(
    () => {
      const { refresh } = optionsRef.current;
      return refresh ? refresh() : queryRef.current.refetch();
    },
    { onError: options.onError }
  );

  // Synchronous guards: `isFetching` only flips on the next render.
  const pendingRef = useRef({ next: false, previous: false });

  // Called inside `.then` so a synchronous throw still clears the guard instead of wedging it.
  const settle = useCallback(
    (fetchPage: () => Promise<unknown>, clearGuard: () => void) => {
      const pending = Promise.resolve().then(fetchPage).finally(clearGuard);
      const { onError } = optionsRef.current;
      if (onError) pending.catch(onError);
    },
    []
  );

  const onEndReached = useCallback(() => {
    const current = queryRef.current;
    if (!current.hasNextPage || current.isFetching || pendingRef.current.next) {
      return;
    }
    pendingRef.current.next = true;
    settle(
      () => current.fetchNextPage(),
      () => {
        pendingRef.current.next = false;
      }
    );
  }, [settle]);

  const onStartReached = useCallback(() => {
    const current = queryRef.current;
    if (
      !current.hasPreviousPage ||
      current.isFetching ||
      pendingRef.current.previous
    ) {
      return;
    }
    pendingRef.current.previous = true;
    settle(
      () => current.fetchPreviousPage(),
      () => {
        pendingRef.current.previous = false;
      }
    );
  }, [settle]);

  return useMemo(
    () => ({ data, refreshing, onRefresh, onEndReached, onStartReached }),
    [data, refreshing, onRefresh, onEndReached, onStartReached]
  );
}
