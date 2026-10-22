import { createHashStore, memoryStorage, type HashEntry } from 'shadowhash';
import type { FeedItem } from 'shadowlist-utils/native';
import { feedItemFromHash, hashFeedItem } from './feedHash';

/*
 * The feed's hashes kept in memory for the session because the macOS example has no MMKV. A
 * screen that opens again draws them before its first page returns, and a cold start draws none.
 */
const store = createHashStore(memoryStorage());

export function rememberFeed(listKey: string, items: ReadonlyArray<FeedItem>) {
  const entries: HashEntry[] = items
    .filter((item) => !item.placeholder)
    .map(hashFeedItem);
  store.remember(listKey, entries);
}

export function recallFeed(listKey: string): FeedItem[] {
  try {
    return store.recall(listKey).map(feedItemFromHash);
  } catch {
    // A hash from another version of the format. The next load writes fresh ones.
    store.forget(listKey);
    return [];
  }
}
