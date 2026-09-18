import { useCallback } from 'react';
import { useMutation, useQueryClient } from '@tanstack/react-query';
import { fetchAssistantHistory, sendAssistantFeedback } from '../api/assistant';

/*
 * The conversation itself is edited locally all the time (streams, regenerations, edits),
 * so it lives in the screen's list controller rather than in the cache. React Query only
 * fetches earlier history, which never changes once loaded, so each page is cached for good.
 */
export function useFetchAssistantHistory() {
  const queryClient = useQueryClient();
  return useCallback(
    (page: number) =>
      queryClient.fetchQuery({
        queryKey: ['assistant', 'history', page],
        queryFn: ({ signal }) => fetchAssistantHistory(page, signal),
        staleTime: Infinity,
      }),
    [queryClient]
  );
}

export const useSendAssistantFeedback = () =>
  useMutation({ mutationFn: sendAssistantFeedback });
