import { createMMKV } from 'react-native-mmkv';
import { createHashStore, type HashEntry } from 'shadowhash';
import type { FeedItem } from 'shadowlist-utils/native';
import { feedItemFromHash, hashFeedItem } from './feedHash';

/*
 * The feed's first rows as hashes, kept on the device. A cold start draws them before the first
 * page returns. A 30 row feed is under 2 KB.
 */
const storage = createMMKV({ id: 'shadowhash' });

const store = createHashStore({
  getString: (key) => storage.getString(key),
  set: (key, value) => storage.set(key, value),
  remove: (key) => {
    storage.remove(key);
  },
});

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
