import type { ChatMessage } from 'shadowlist-utils/native';
import { generateUniqueId } from '../fixtures/common';
import { CHAT_ME, buildChatMessage } from '../fixtures/chat';
import { Collection, type CursorPage, type PageCursor } from './Collection';
import {
  network,
  request,
  RequestFailedError,
  type RequestSignal,
} from './network';

const PAGE_SIZE = 50;
const HISTORY_SIZE = 1000;

let generatedCount = 0;
const generateMessages = (count: number) =>
  Array.from({ length: count }, () => buildChatMessage(generatedCount++));

const messages = new Collection<ChatMessage>({
  seed: () => generateMessages(HISTORY_SIZE),
});

// Opens on the newest messages; `{ before }` cursors walk back through history.
export function fetchChatPage(
  cursor: PageCursor | undefined,
  signal?: RequestSignal
): Promise<CursorPage<ChatMessage>> {
  return request(
    () => messages.page(cursor, { limit: PAGE_SIZE, from: 'end' }),
    signal
  );
}

/*
 * The client picks the id and the server keeps it. The optimistic bubble and the stored
 * message then share one list key, so confirming the send never remounts the row.
 */
export function createOutgoingMessage(text: string): ChatMessage {
  return {
    id: generateUniqueId(),
    author: CHAT_ME,
    isOwn: true,
    text,
    createdAt: Date.now(),
    status: 'sending',
  };
}

// Text containing this always fails to send, to try the retry flow by hand.
const FAIL_TAG = '#fail';

export function sendChatMessage(message: ChatMessage): Promise<ChatMessage> {
  return request(() => {
    if (
      message.text?.includes(FAIL_TAG) ||
      Math.random() < network.sendFailureRate
    ) {
      throw new RequestFailedError();
    }
    const stored: ChatMessage = { ...message, status: 'sent' };
    messages.insertAtEnd([stored]);
    return stored;
  });
}

// A stand-in for the socket that pushes messages from the other participants.
const incomingListeners = new Set<(incoming: ChatMessage[]) => void>();

export function subscribeToIncomingMessages(
  listener: (incoming: ChatMessage[]) => void
): () => void {
  incomingListeners.add(listener);
  return () => {
    incomingListeners.delete(listener);
  };
}

export function simulateIncomingMessages(count: number): Promise<void> {
  return request(() => {
    const incoming = generateMessages(count);
    messages.insertAtEnd(incoming);
    incomingListeners.forEach((listener) => listener(incoming));
  });
}
