import { useEffect } from 'react';
import {
  useMutation,
  useQueryClient,
  type QueryClient,
} from '@tanstack/react-query';
import { upsertInfiniteItems } from 'shadowlist-utils';
import type { ChatMessage } from 'shadowlist-utils/native';
import {
  fetchChatPage,
  sendChatMessage,
  subscribeToIncomingMessages,
} from '../api/chat';
import { useCursorInfiniteQuery, type CursorData } from './infinite';

const messagesKey = ['chat', 'messages'] as const;

export const useChatMessagesQuery = () =>
  useCursorInfiniteQuery({
    queryKey: messagesKey,
    fetchPage: fetchChatPage,
    // The socket keeps the thread current; a refetch would re-walk every history page.
    staleTime: Infinity,
  });

/*
 * Upsert, not append. A send's confirmation, failure and retry all rewrite the same id in
 * place, and a message the socket delivers twice never adds a duplicate key.
 */
async function writeToThread(
  queryClient: QueryClient,
  messages: ChatMessage[]
) {
  /*
   * A history page still loading was built from the old pages and would overwrite this write
   * when it lands. Cancel it first, and the next start reached event loads it again.
   */
  await queryClient.cancelQueries({ queryKey: messagesKey });
  queryClient.setQueryData<CursorData<ChatMessage>>(messagesKey, (data) =>
    upsertInfiniteItems(data, messages)
  );
}

export function useSendChatMessage() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: sendChatMessage,
    /*
     * The bubble shows at once as sending, and the response confirms it in place with the same id.
     * A failure keeps the bubble marked failed, so the text is never lost and can be retried.
     */
    onMutate: (message) =>
      writeToThread(queryClient, [{ ...message, status: 'sending' }]),
    onSuccess: (stored) => writeToThread(queryClient, [stored]),
    onError: (_error, message) =>
      writeToThread(queryClient, [{ ...message, status: 'failed' }]),
  });
}

/*
 * Writes pushed messages straight into the cache, as a socket handler would.
 */
export function useIncomingChatMessages() {
  const queryClient = useQueryClient();
  useEffect(
    () =>
      subscribeToIncomingMessages((messages) => {
        writeToThread(queryClient, messages);
      }),
    [queryClient]
  );
}
