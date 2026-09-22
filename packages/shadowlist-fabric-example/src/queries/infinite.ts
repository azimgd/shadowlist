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

/*
 * The cached value of every cursor-paginated list in the app.
 */
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

/*
 * An infinite query over a CursorPage endpoint, pageable in both directions.
 */
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
     * Rows keep their object identity when a prepend or a history page moves them, so only new
     * rows render. The default sharing matches rows by position.
     */
    structuralSharing: shareInfiniteItemsById,
    // Set only when given, since an explicit undefined would override the client default.
    ...(staleTime === undefined ? {} : { staleTime }),
  });
}

/*
 * Pull to refresh for a long infinite list. A refetch reloads every loaded page one by one, so
 * keep only the first page and make it one request. The reader at the top never sees the
 * dropped rows.
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
 * A create call whose rows belong at the top of a feed. They are merged into the first page
 * from the response without a refetch, and the list keeps the reader's position as they land.
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
