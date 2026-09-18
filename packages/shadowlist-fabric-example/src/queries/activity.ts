import { useMutation, useQueryClient } from '@tanstack/react-query';
import { removeInfiniteItems } from 'shadowlist-utils';
import type { ActivityItem } from 'shadowlist-utils/native';
import { deleteActivities, fetchActivityPage } from '../api/activity';
import {
  useCursorInfiniteQuery,
  useRefreshFirstPage,
  type CursorData,
} from './infinite';

const activityKey = ['activity'] as const;

export const useActivityQuery = () =>
  useCursorInfiniteQuery({
    queryKey: activityKey,
    fetchPage: fetchActivityPage,
  });

export const useRefreshActivity = () => useRefreshFirstPage(activityKey);

// Rows leave the list at once and come back if the server refuses.
export function useDeleteActivities() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: deleteActivities,
    onMutate: async (ids) => {
      await queryClient.cancelQueries({ queryKey: activityKey });
      const previous =
        queryClient.getQueryData<CursorData<ActivityItem>>(activityKey);
      const removed = new Set(ids);
      queryClient.setQueryData<CursorData<ActivityItem>>(activityKey, (data) =>
        removeInfiniteItems(data, (item) => removed.has(item.id))
      );
      return { previous };
    },
    onError: (_error, _ids, context) =>
      queryClient.setQueryData(activityKey, context?.previous),
  });
}
