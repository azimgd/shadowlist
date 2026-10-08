import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { shareItemsById } from 'shadowlist-utils';
import type { InboxMessage } from 'shadowlist-utils/native';
import {
  deleteMessages,
  fetchInbox,
  receiveMail,
  updateMessages,
} from '../api/inbox';

const inboxKey = ['inbox'] as const;

export const useInboxQuery = () =>
  useQuery({
    queryKey: inboxKey,
    queryFn: ({ signal }) => fetchInbox(signal),
    structuralSharing: shareItemsById,
  });

export function useReceiveMail() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: receiveMail,
    onSuccess: (received) =>
      queryClient.setQueryData<InboxMessage[]>(
        inboxKey,
        (data) => data && [...received, ...data]
      ),
  });
}

/*
 * Edits and deletes show at once and roll back if the server refuses.
 */
export function useUpdateMessages() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: ({
      ids,
      change,
    }: {
      ids: ReadonlyArray<string>;
      change: Partial<Pick<InboxMessage, 'read' | 'flagged'>>;
    }) => updateMessages(ids, change),
    onMutate: async ({ ids, change }) => {
      await queryClient.cancelQueries({ queryKey: inboxKey });
      const previous = queryClient.getQueryData<InboxMessage[]>(inboxKey);
      const changed = new Set(ids);
      queryClient.setQueryData<InboxMessage[]>(inboxKey, (data) =>
        data?.map((item) =>
          changed.has(item.id) ? { ...item, ...change } : item
        )
      );
      return { previous };
    },
    onError: (_error, _variables, context) =>
      queryClient.setQueryData(inboxKey, context?.previous),
  });
}

export function useDeleteMessages() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: deleteMessages,
    onMutate: async (ids) => {
      await queryClient.cancelQueries({ queryKey: inboxKey });
      const previous = queryClient.getQueryData<InboxMessage[]>(inboxKey);
      const removed = new Set(ids);
      queryClient.setQueryData<InboxMessage[]>(inboxKey, (data) =>
        data?.filter((item) => !removed.has(item.id))
      );
      return { previous };
    },
    onError: (_error, _ids, context) =>
      queryClient.setQueryData(inboxKey, context?.previous),
  });
}
