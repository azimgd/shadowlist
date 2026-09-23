import { describe, expect, it } from '@jest/globals';
import {
  describeRows,
  planDataSync,
  type DataSyncPlan,
  type SyncedRows,
} from '../native/dataSync';

interface Item {
  id: string;
  kind?: string;
}

interface StoreRow {
  key: string;
  item: Item;
  template: string;
}

const byId = (item: Item) => item.id;
const byKind = (item: Item) => item.kind ?? '';

/*
 * What the engine holds after setData: first copy of each key, in order.
 */
function setDataModel(rows: SyncedRows<Item>): StoreRow[] {
  const seen = new Set<string>();
  const store: StoreRow[] = [];
  rows.items.forEach((item, index) => {
    const key = rows.keys[index]!;
    if (seen.has(key)) return;
    seen.add(key);
    store.push({ key, item, template: rows.templates?.[index] ?? '' });
  });
  return store;
}

/*
 * The engine's insertItems / removeItems / updateItem(replace) on the model.
 */
function applyPlan(
  store: StoreRow[],
  plan: DataSyncPlan,
  next: SyncedRows<Item>
): StoreRow[] {
  switch (plan.kind) {
    case 'none':
      return store;
    case 'update':
      return store.map((row, index) =>
        plan.indices.includes(index)
          ? {
              key: row.key,
              item: next.items[index]!,
              template: next.templates?.[index] || row.template,
            }
          : row
      );
    case 'insert': {
      const inserted = next.items
        .slice(plan.at, plan.at + plan.count)
        .map((item, offset) => ({
          key: next.keys[plan.at + offset]!,
          item,
          template: next.templates?.[plan.at + offset] ?? '',
        }));
      return [...store.slice(0, plan.at), ...inserted, ...store.slice(plan.at)];
    }
    case 'remove': {
      const doomed = new Set(plan.keys);
      return store.filter((row) => !doomed.has(row.key));
    }
    case 'reset':
      return setDataModel(next);
  }
}

function items(...ids: string[]): Item[] {
  return ids.map((id) => ({ id }));
}

describe('planDataSync', () => {
  it('does nothing for the same rows in a new array', () => {
    const list = items('a', 'b', 'c');
    const plan = planDataSync(
      describeRows(list, byId, null),
      describeRows([...list], byId, null),
      8
    );
    expect(plan).toEqual({ kind: 'none' });
  });

  it('updates only the edited rows', () => {
    const list = items('a', 'b', 'c');
    const next = [list[0]!, { id: 'b' }, list[2]!];
    expect(
      planDataSync(
        describeRows(list, byId, null),
        describeRows(next, byId, null),
        8
      )
    ).toEqual({ kind: 'update', indices: [1] });
  });

  it('replaces the store past the edit cap', () => {
    const list = items('a', 'b', 'c');
    expect(
      planDataSync(
        describeRows(list, byId, null),
        describeRows(items('a', 'b', 'c'), byId, null),
        2
      )
    ).toEqual({ kind: 'reset' });
  });

  it('finds a prepend, an append and an insert in the middle', () => {
    const list = items('a', 'b', 'c');
    const [a, b, c] = list;
    const plan = (next: Item[]) =>
      planDataSync(
        describeRows(list, byId, null),
        describeRows(next, byId, null),
        8
      );
    expect(plan([{ id: 'x' }, { id: 'y' }, a!, b!, c!])).toEqual({
      kind: 'insert',
      at: 0,
      count: 2,
    });
    expect(plan([a!, b!, c!, { id: 'x' }])).toEqual({
      kind: 'insert',
      at: 3,
      count: 1,
    });
    expect(plan([a!, { id: 'x' }, b!, c!])).toEqual({
      kind: 'insert',
      at: 1,
      count: 1,
    });
    // An insert plus an edit is two changes, so the store is replaced.
    expect(plan([{ id: 'x' }, a!, { id: 'b' }, c!])).toEqual({
      kind: 'reset',
    });
  });

  it('finds removed rows', () => {
    const list = items('a', 'b', 'c', 'd');
    const [a, , c] = list;
    expect(
      planDataSync(
        describeRows(list, byId, null),
        describeRows([a!, c!], byId, null),
        8
      )
    ).toEqual({ kind: 'remove', keys: ['b', 'd'] });
  });

  it('replaces the store for moves, duplicates and template changes', () => {
    const list = items('a', 'b', 'c');
    const [a, b, c] = list;
    const previous = describeRows(list, byId, null);
    expect(
      planDataSync(previous, describeRows([b!, a!, c!], byId, null), 8)
    ).toEqual({ kind: 'reset' });
    expect(
      planDataSync(previous, describeRows([a!, b!, c!, a!], byId, null), 8)
    ).toEqual({ kind: 'reset' });
    expect(planDataSync(previous, describeRows(list, byId, byKind), 8)).toEqual(
      { kind: 'reset' }
    );
  });

  it('replaces the store when a row loses its template', () => {
    const list: Item[] = [{ id: 'a', kind: 'big' }];
    expect(
      planDataSync(
        describeRows(list, byId, byKind),
        describeRows([{ id: 'a' }], byId, byKind),
        8
      )
    ).toEqual({ kind: 'reset' });
  });

  it('leaves the store as setData would, for random edits', () => {
    let seed = 7;
    const random = () => {
      seed = (seed * 1103515245 + 12345) % 2147483648;
      return seed / 2147483648;
    };
    let nextId = 0;
    const fresh = (): Item => ({
      id: `n${nextId++}`,
      kind: random() < 0.5 ? 'a' : 'b',
    });
    let current: Item[] = Array.from({ length: 12 }, fresh);
    let synced = describeRows(current, byId, byKind);
    let store = setDataModel(synced);
    const plans = new Set<string>();

    for (let step = 0; step < 2000; step++) {
      const next = [...current];
      const change = Math.floor(random() * 6);
      if (!synced.unique) {
        // Drop the duplicate pushed last step.
        next.pop();
      } else if (change === 0 && next.length > 0) {
        const index = Math.floor(random() * next.length);
        next[index] = { ...next[index]!, kind: random() < 0.5 ? 'a' : 'b' };
      } else if (change === 1) {
        const at = Math.floor(random() * (next.length + 1));
        next.splice(at, 0, ...Array.from({ length: 1 + (step % 3) }, fresh));
      } else if (change === 2 && next.length > 0) {
        next.splice(Math.floor(random() * next.length), 1 + (step % 2));
      } else if (change === 3 && next.length > 1) {
        const [moved] = next.splice(0, 1);
        next.push(moved!);
      } else if (change === 4 && next.length > 0) {
        next.push({ ...next[0]! });
      }
      if (next.length > 40) next.splice(0, 20);

      const rows = describeRows(next, byId, byKind);
      const plan = synced.unique
        ? planDataSync(synced, rows, 8)
        : ({ kind: 'reset' } as const);
      plans.add(plan.kind);
      store = applyPlan(store, plan, rows);
      expect(store).toEqual(setDataModel(rows));
      current = next;
      synced = rows;
    }
    expect([...plans].sort()).toEqual([
      'insert',
      'none',
      'remove',
      'reset',
      'update',
    ]);
  });
});
