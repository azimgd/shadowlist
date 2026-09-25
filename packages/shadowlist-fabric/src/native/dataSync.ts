/*
 * Turns a new controlled data array into the smallest store change. setData sends every item
 * over JSI and the engine converts and compares all of them, so a one row edit in a long list
 * paid for the whole list. Common changes become one store call instead: some items edited
 * in place, one block inserted, or some rows removed. Anything else replaces the store.
 *
 * No React or binding in here, so it can be tested on its own.
 */

/*
 * What the store holds after the last sync: the items, their keys and their template names
 * (null when the list has no templateOf). unique is false when a key repeats, since the
 * store keeps only the first copy and no longer lines up with the array.
 */
export interface SyncedRows<ItemT> {
  items: ReadonlyArray<ItemT>;
  keys: string[];
  templates: string[] | null;
  unique: boolean;
}

export type DataSyncPlan =
  | { kind: 'none' }
  // Replace the item at each index, keys unchanged.
  | { kind: 'update'; indices: number[] }
  // count new rows at index at, everything else unchanged.
  | { kind: 'insert'; at: number; count: number }
  // Drop these keys, everything else unchanged and in order.
  | { kind: 'remove'; keys: string[] }
  | { kind: 'reset' };

const RESET: DataSyncPlan = { kind: 'reset' };
const NONE: DataSyncPlan = { kind: 'none' };

export function describeRows<ItemT>(
  items: ReadonlyArray<ItemT>,
  keyExtractor: (item: ItemT, index: number) => string,
  templateOf: ((item: ItemT, index: number) => string) | null
): SyncedRows<ItemT> {
  const keys = new Array<string>(items.length);
  const templates = templateOf ? new Array<string>(items.length) : null;
  const seen = new Set<string>();
  for (let index = 0; index < items.length; index++) {
    const item = items[index]!;
    const key = keyExtractor(item, index);
    keys[index] = key;
    seen.add(key);
    if (templates && templateOf) templates[index] = templateOf(item, index);
  }
  return { items, keys, templates, unique: seen.size === items.length };
}

function sameRow<ItemT>(
  previous: SyncedRows<ItemT>,
  previousIndex: number,
  next: SyncedRows<ItemT>,
  nextIndex: number
): boolean {
  return (
    previous.items[previousIndex] === next.items[nextIndex] &&
    (previous.templates === null ||
      previous.templates[previousIndex] === next.templates![nextIndex])
  );
}

/*
 * maxUpdates caps the in place path. Each edit is its own store call and commit request, so
 * past a handful one setData is cheaper.
 */
export function planDataSync<ItemT>(
  previous: SyncedRows<ItemT>,
  next: SyncedRows<ItemT>,
  maxUpdates: number
): DataSyncPlan {
  if (!previous.unique || !next.unique) return RESET;
  if ((previous.templates === null) !== (next.templates === null)) return RESET;
  const previousCount = previous.keys.length;
  const nextCount = next.keys.length;

  if (nextCount === previousCount) {
    const indices: number[] = [];
    for (let index = 0; index < nextCount; index++) {
      if (previous.keys[index] !== next.keys[index]) return RESET;
      if (sameRow(previous, index, next, index)) continue;
      /*
       * An empty template name means keep the row's template in updateItem, so a row going
       * back to no template can only be written by setData.
       */
      if (
        next.templates !== null &&
        next.templates[index] === '' &&
        previous.templates![index] !== ''
      ) {
        return RESET;
      }
      indices.push(index);
      if (indices.length > maxUpdates) return RESET;
    }
    return indices.length === 0 ? NONE : { kind: 'update', indices };
  }

  if (nextCount > previousCount) {
    const count = nextCount - previousCount;
    let at = 0;
    while (at < previousCount && previous.keys[at] === next.keys[at]) at++;
    for (let index = 0; index < at; index++) {
      if (!sameRow(previous, index, next, index)) return RESET;
    }
    for (let index = at; index < previousCount; index++) {
      if (
        previous.keys[index] !== next.keys[index + count] ||
        !sameRow(previous, index, next, index + count)
      ) {
        return RESET;
      }
    }
    return { kind: 'insert', at, count };
  }

  // Fewer rows. The new keys must be the old ones in order with some left out.
  const removed: string[] = [];
  let previousIndex = 0;
  for (let nextIndex = 0; nextIndex < nextCount; nextIndex++) {
    const key = next.keys[nextIndex];
    while (
      previousIndex < previousCount &&
      previous.keys[previousIndex] !== key
    ) {
      removed.push(previous.keys[previousIndex]!);
      previousIndex++;
    }
    if (previousIndex === previousCount) return RESET;
    if (!sameRow(previous, previousIndex, next, nextIndex)) return RESET;
    previousIndex++;
  }
  while (previousIndex < previousCount) {
    removed.push(previous.keys[previousIndex]!);
    previousIndex++;
  }
  return { kind: 'remove', keys: removed };
}
