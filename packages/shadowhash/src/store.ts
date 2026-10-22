/*
 * Hashes kept on the device between launches. A list remembers the hashes of its first rows
 * when they load, and on the next cold start draws placeholders of those exact shapes before
 * the first request returns.
 */
export interface HashStorage {
  getString(key: string): string | undefined;
  set(key: string, value: string): void;
  remove(key: string): void;
}

export interface HashEntry {
  id: string;
  hash: string;
}

export interface HashStore {
  remember(listKey: string, entries: ReadonlyArray<HashEntry>): void;
  recall(listKey: string): HashEntry[];
  forget(listKey: string): void;
}

/*
 * limit is the number of rows kept per list. A screen and a half is enough to cover a cold start.
 */
export interface HashStoreOptions {
  limit?: number;
  prefix?: string;
}

/*
 * A store backed by an MMKV instance, localStorage behind a small adapter, or memoryStorage.
 */
export function createHashStore(
  storage: HashStorage,
  { limit = 30, prefix = 'shadowhash:' }: HashStoreOptions = {}
): HashStore {
  return {
    remember(listKey, entries) {
      // id and hash pairs flattened. A 30 row list is one short JSON array.
      const flat = entries
        .slice(0, limit)
        .flatMap(({ id, hash }) => [id, hash]);
      storage.set(prefix + listKey, JSON.stringify(flat));
    },
    recall(listKey) {
      const raw = storage.getString(prefix + listKey);
      if (raw === undefined) return [];
      try {
        const flat: unknown = JSON.parse(raw);
        if (!Array.isArray(flat)) return [];
        const entries: HashEntry[] = [];
        for (let index = 0; index + 1 < flat.length; index += 2) {
          const [id, hash] = [flat[index], flat[index + 1]];
          if (typeof id === 'string' && typeof hash === 'string')
            entries.push({ id, hash });
        }
        return entries;
      } catch {
        return [];
      }
    },
    forget(listKey) {
      storage.remove(prefix + listKey);
    },
  };
}

export function memoryStorage(): HashStorage {
  const values = new Map<string, string>();
  return {
    getString: (key) => values.get(key),
    set: (key, value) => {
      values.set(key, value);
    },
    remove: (key) => {
      values.delete(key);
    },
  };
}
