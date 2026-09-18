import { useCallback } from 'react';
import {
  useInfiniteQuery,
  useMutation,
  useQueryClient,
  type InfiniteData,
  type QueryKey,
} from '@tanstack/react-query';
import {
  prependInfiniteItems,
  shareInfiniteItemsById,
  trimInfinitePages,
} from 'shadowlist-utils';
import {
  nextPageCursor,
  previousPageCursor,
  type CursorPage,
  type PageCursor,
} from '../api/Collection';
import type { RequestSignal } from '../api/network';

// The cached value of every cursor-paginated list in the app.
export type CursorData<ItemT> = InfiniteData<
  CursorPage<ItemT>,
  PageCursor | undefined
>;

interface CursorInfiniteQueryOptions<ItemT> {
  queryKey: QueryKey;
  fetchPage: (
    cursor: PageCursor | undefined,
    signal: RequestSignal
  ) => Promise<CursorPage<ItemT>>;
  staleTime?: number;
}

// An infinite query over a CursorPage endpoint, pageable in both directions.
export function useCursorInfiniteQuery<ItemT>({
  queryKey,
  fetchPage,
  staleTime,
}: CursorInfiniteQueryOptions<ItemT>) {
  return useInfiniteQuery({
    queryKey,
    queryFn: ({ pageParam, signal }) => fetchPage(pageParam, signal),
    initialPageParam: undefined as PageCursor | undefined,
    getNextPageParam: nextPageCursor,
    getPreviousPageParam: previousPageCursor,
    /*
     * Rows keep their object identity when a prepend or a page of history moves them, so
     * only the new rows render; the default sharing matches rows by position.
     */
    structuralSharing: shareInfiniteItemsById,
    // Only when given: an explicit undefined would override the client default.
    ...(staleTime === undefined ? {} : { staleTime }),
  });
}

/*
 * Pull-to-refresh for a long infinite list. Refetching refetches every loaded page in turn,
 * so after a deep scroll it would take one round trip per page; keeping only the first page
 * makes it one request, and the reader pulling at the top never sees the rows that drop.
 */
export function useRefreshFirstPage(queryKey: QueryKey) {
  const queryClient = useQueryClient();
  return useCallback(() => {
    queryClient.setQueryData<CursorData<unknown>>(queryKey, (data) =>
      trimInfinitePages(data, 1)
    );
    return queryClient.refetchQueries({ queryKey, exact: true });
  }, [queryClient, queryKey]);
}

/*
 * A create call whose rows belong at the top of a feed: merged into the first page from the
 * response, without refetching. The list keeps the reader's position as they land.
 */
export function usePrependMutation<ItemT, VariablesT>(
  queryKey: QueryKey,
  mutationFn: (variables: VariablesT) => Promise<ItemT[]>
) {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn,
    onSuccess: (created) =>
      queryClient.setQueryData<CursorData<ItemT>>(queryKey, (data) =>
        prependInfiniteItems(data, created)
      ),
  });
}
