import type { InboxMessage } from 'shadowlist-utils/native';
import { generateInboxMessage } from '../fixtures/inbox';
import { Collection } from './Collection';
import { request, type RequestSignal } from './network';

const INBOX_COUNT = 300;

let generatedCount = 0;

/*
 * A mailbox small enough to fetch whole. New mail lands on top.
 */
const inbox = new Collection<InboxMessage>({
  seed: () =>
    Array.from({ length: INBOX_COUNT }, () =>
      generateInboxMessage(generatedCount++)
    ),
});

export function fetchInbox(signal?: RequestSignal): Promise<InboxMessage[]> {
  return request(() => inbox.all(), signal);
}

export function receiveMail(count: number): Promise<InboxMessage[]> {
  return request(() => {
    const now = Date.now();
    const received = Array.from({ length: count }, (_, index) => ({
      ...generateInboxMessage(generatedCount++, now),
      receivedAt: now - index * 1000,
      read: false,
    }));
    inbox.insertAtStart(received);
    return received;
  });
}

export function updateMessages(
  ids: ReadonlyArray<string>,
  change: Partial<Pick<InboxMessage, 'read' | 'flagged'>>
): Promise<void> {
  return request(() => {
    ids.forEach((id) => inbox.update(id, (item) => ({ ...item, ...change })));
  });
}

export function deleteMessages(ids: ReadonlyArray<string>): Promise<void> {
  return request(() => inbox.remove(ids));
}
