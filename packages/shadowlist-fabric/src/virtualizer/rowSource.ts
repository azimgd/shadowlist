import { useMemo, useRef } from 'react';
import { nextVersion, VERSION_GONE, type ListStore } from './nativeStore';

/*
 * The rows a list shows, however they are stored. ShadowList reads its data through this.
 * SectionList and TreeList then need not build a flat array of every row: they answer for the
 * rows asked about, which are the mounted ones, and keep the full row keys in the native store.
 * A source never changes. New data makes a new source.
 *
 * indexOfKey gives a key's row or -1. hint is where it probably is, like the row it had before
 * a data change. The rows around it are tried first and the full lookup only runs when they
 * miss. data is the plain data array, which onMoveItem uses, or null for sources without one.
 * keysVersion is the version the keys were written as in the native store, or 0 when they go
 * to native in propKeys instead. nativeSticky means native works out the rows that stick from
 * the keys and stickyIndices stays empty. Without the store, stickyIndices carries them.
 */
export interface RowSource<ItemT> {
  readonly count: number;
  itemAt(index: number): ItemT | undefined;
  keyAt(index: number): string | undefined;
  indexOfKey(key: string, hint?: number): number;
  indexOfItem(item: unknown): number;
  readonly data: ReadonlyArray<ItemT> | null;
  readonly keysVersion: number;
  readonly propKeys: ReadonlyArray<string>;
  readonly nativeSticky: boolean;
  readonly stickyIndices: ReadonlyArray<number> | null;
}

export const NO_KEYS: ReadonlyArray<string> = [];

/*
 * No rows, before a list's first render.
 */
export const EMPTY_SOURCE: RowSource<never> = {
  count: 0,
  itemAt: () => undefined,
  keyAt: () => undefined,
  indexOfKey: () => -1,
  indexOfItem: () => -1,
  data: [],
  keysVersion: 0,
  propKeys: NO_KEYS,
  nativeSticky: false,
  stickyIndices: null,
};

/*
 * What stays the same while a source's keys do: the native version, or the keys sent in props.
 * Hooks that only depend on keys rerun on it instead of on every source.
 */
export function keysIdentityOf(source: RowSource<unknown>): unknown {
  return source.keysVersion > 0 ? source.keysVersion : source.propKeys;
}

/*
 * How far around a hint a key is looked for before the full lookup. The native store uses the
 * same window, see RowKeysSnapshot::HINT_WINDOW.
 */
export const HINT_WINDOW = 64;

/*
 * Looks for key around hint with keyAt. -1 when it is not within HINT_WINDOW.
 */
export function findKeyNear(
  key: string,
  hint: number | undefined,
  count: number,
  keyAt: (index: number) => string | undefined
): number {
  if (hint === undefined || hint < 0 || count === 0) return -1;
  const center = Math.min(hint, count - 1);
  if (keyAt(center) === key) return center;
  for (let distance = 1; distance <= HINT_WINDOW; distance++) {
    const below = center - distance;
    const above = center + distance;
    if (below < 0 && above >= count) break;
    if (below >= 0 && keyAt(below) === key) return below;
    if (above < count && keyAt(above) === key) return above;
  }
  return -1;
}

/*
 * A key's first index in keys, through an index built on first use and kept per array. For
 * sources without the native store.
 */
const keyIndexes = new WeakMap<ReadonlyArray<string>, Map<string, number>>();

export function indexOfKeyIn(keys: ReadonlyArray<string>, key: string): number {
  let index = keyIndexes.get(keys);
  if (index === undefined) {
    index = new Map();
    for (let position = 0; position < keys.length; position++) {
      const value = keys[position]!;
      if (!index.has(value)) index.set(value, position);
    }
    keyIndexes.set(keys, index);
  }
  return index.get(key) ?? -1;
}

/*
 * A key's row from the native store's version, or from keys when the store lost the version or
 * there is none.
 */
export function lookUpKey(
  store: ListStore | null,
  version: number,
  key: string,
  fallbackKeys: () => ReadonlyArray<string>
): number {
  if (store !== null && version > 0) {
    const found = store.module.indexOfKey(store.handle, version, key, -1);
    if (found !== VERSION_GONE) return found;
  }
  return indexOfKeyIn(fallbackKeys(), key);
}

/*
 * The keys of data and how they differ from the previous keys: the rows before start and the
 * last suffix rows are the same.
 */
interface KeyedData {
  keys: string[];
  same: boolean;
  start: number;
  suffix: number;
}

function keysOf<ItemT>(
  data: ReadonlyArray<ItemT>,
  keyExtractor: (item: ItemT, index: number) => string,
  previous: ReadonlyArray<string> | null
): KeyedData {
  const keys = new Array<string>(data.length);
  let start = previous === null ? 0 : -1;
  for (let index = 0; index < data.length; index++) {
    const key = keyExtractor(data[index]!, index);
    keys[index] = key;
    if (start < 0 && previous![index] !== key) start = index;
  }
  if (previous === null) return { keys, same: false, start: 0, suffix: 0 };
  if (start < 0) {
    if (previous.length === keys.length) {
      return { keys, same: true, start: keys.length, suffix: 0 };
    }
    start = Math.min(previous.length, keys.length);
  }
  let suffix = 0;
  const limit = Math.min(previous.length, keys.length) - start;
  while (
    suffix < limit &&
    previous[previous.length - 1 - suffix] === keys[keys.length - 1 - suffix]
  ) {
    suffix++;
  }
  return { keys, same: false, start, suffix };
}

/*
 * Writes keys as a new version: the changed middle against the previous version when there is
 * one, or all of them. Returns the version.
 */
export function writeKeys(
  store: ListStore,
  keyed: KeyedData,
  previousVersion: number,
  previousLength: number
): number {
  const version = nextVersion(store);
  const { keys, start, suffix } = keyed;
  if (previousVersion > 0) {
    const deleteCount = previousLength - start - suffix;
    const inserted = keys.slice(start, keys.length - suffix);
    if (
      store.module.spliceKeys(
        store.handle,
        version,
        previousVersion,
        start,
        deleteCount,
        inserted
      )
    ) {
      return version;
    }
  }
  store.module.setKeys(store.handle, version, keys);
  return version;
}

export interface PlainSourceCache<ItemT> {
  source: RowSource<ItemT>;
  keys: string[];
  keyExtractor: (item: ItemT, index: number) => string;
}

/*
 * Duplicate keys already warned about, so a list that keeps them warns once per key.
 */
const warnedDuplicateKeys = new Set<string>();

function warnDuplicateKeys(keys: ReadonlyArray<string>): void {
  const seen = new Set<string>();
  for (const key of keys) {
    if (!seen.has(key)) {
      seen.add(key);
      continue;
    }
    if (!warnedDuplicateKeys.has(key)) {
      warnedDuplicateKeys.add(key);
      console.warn(
        `ShadowList: two items have the key '${key}'. Every item needs its own key, or rows ` +
          'swap content and the list cannot keep what is on screen in place. Check keyExtractor.'
      );
    }
    return;
  }
}

/*
 * The source of a plain data array after previous. keyExtractor runs over every row on a data
 * change, since it is the caller's. Native gets only the keys that changed. When the keys come
 * out the same, like for an item edited in place, the previous version is kept: native gets no
 * new keys and the hooks keyed on the keys version don't rerun.
 */
export function nextPlainSource<ItemT>(
  previous: PlainSourceCache<ItemT> | null,
  data: ReadonlyArray<ItemT>,
  keyExtractor: (item: ItemT, index: number) => string,
  store: ListStore | null
): PlainSourceCache<ItemT> {
  /*
   * The same data again gives the same source, like a Strict Mode second run or a parent render
   * that passes a new inline keyExtractor. The same items keep their keys.
   */
  if (previous !== null && previous.source.data === data) {
    return previous.keyExtractor === keyExtractor
      ? previous
      : { ...previous, keyExtractor };
  }
  const keyed = keysOf(data, keyExtractor, previous?.keys ?? null);
  if (__DEV__ && !keyed.same) {
    warnDuplicateKeys(keyed.keys);
  }
  const keys = keyed.same ? previous!.keys : keyed.keys;
  let version = 0;
  if (store !== null) {
    version =
      keyed.same && previous !== null
        ? previous.source.keysVersion
        : writeKeys(
            store,
            keyed,
            previous?.source.keysVersion ?? 0,
            previous?.keys.length ?? 0
          );
  }
  return {
    source: plainSource(data, keys, store, version),
    keys,
    keyExtractor,
  };
}

export function usePlainSource<ItemT>(
  data: ReadonlyArray<ItemT>,
  keyExtractor: (item: ItemT, index: number) => string,
  store: ListStore | null
): RowSource<ItemT> {
  const cacheRef = useRef<PlainSourceCache<ItemT> | null>(null);
  return useMemo(() => {
    cacheRef.current = nextPlainSource(
      cacheRef.current,
      data,
      keyExtractor,
      store
    );
    return cacheRef.current.source;
  }, [data, keyExtractor, store]);
}

export function plainSource<ItemT>(
  data: ReadonlyArray<ItemT>,
  keys: ReadonlyArray<string>,
  store: ListStore | null,
  version: number
): RowSource<ItemT> {
  const keyAt = (index: number) => keys[index];
  return {
    count: data.length,
    itemAt: (index) => data[index],
    keyAt,
    indexOfKey(key, hint) {
      const near = findKeyNear(key, hint, keys.length, keyAt);
      if (near >= 0) return near;
      return lookUpKey(store, version, key, () => keys);
    },
    indexOfItem: (item) => data.indexOf(item as ItemT),
    data,
    keysVersion: version,
    propKeys: version > 0 ? NO_KEYS : keys,
    nativeSticky: false,
    stickyIndices: null,
  };
}
