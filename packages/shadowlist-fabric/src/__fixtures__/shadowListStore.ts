/*
 * A JS stand-in for the native ShadowListStore module in tests, with the same answers as
 * host/RowKeyStore for what the tests use: versions, splices against a base, sections with
 * reused rows and sticky headers, tree splices, hinted lookups, size specs and the selection.
 * The C++ store and ListSelection have their own tests in shadowlist-core-tests, which these
 * answers follow.
 */
import type {
  NativeStoreHandle,
  ShadowListStoreModule,
} from '../virtualizer/nativeStore';
import type { ItemSizeSpec } from '../types';
import {
  footerRowKey,
  headerRowKey,
  itemRowKey,
} from '../virtualizer/sectionKeys';

export interface FakeSnapshot {
  keys: string[];
  sticky: number[];
  kind: 'plain' | 'sections' | 'tree';
  sectionKeys: string[];
  sectionRows: { start: number; count: number }[];
  depths: number[];
}

export interface FakeStore {
  id: number;
  versions: Map<number, FakeSnapshot>;
  specs: Map<number, { keys: string[]; specs: ItemSizeSpec[] }>;
  selected: string[];
  multiple: boolean;
}

export interface FakeStoreModule extends ShadowListStoreModule {
  stores: FakeStore[];
  calls: string[];
}

function plain(keys: string[]): FakeSnapshot {
  return {
    keys,
    sticky: [],
    kind: 'plain',
    sectionKeys: [],
    sectionRows: [],
    depths: [],
  };
}

function selectOnly(store: FakeStore, key: string): string[] | null {
  if (store.selected.length === 1 && store.selected[0] === key) return null;
  store.selected = [key];
  return [key];
}

export function createFakeStoreModule(): FakeStoreModule {
  const stores: FakeStore[] = [];
  const calls: string[] = [];
  const storeOf = (handle: NativeStoreHandle) => handle as unknown as FakeStore;
  const subtreeEnd = (depths: number[], row: number) => {
    let end = row + 1;
    while (end < depths.length && depths[end]! > depths[row]!) end++;
    return end;
  };
  const module = {
    stores,
    calls,
    getConstants: () => ({}),
    createStore() {
      const store: FakeStore = {
        id: stores.length + 1,
        versions: new Map(),
        specs: new Map(),
        selected: [],
        multiple: false,
      };
      stores.push(store);
      return store as unknown as NativeStoreHandle;
    },
    setKeys(handle, version, keys) {
      calls.push(`setKeys ${version} ${keys.length}`);
      storeOf(handle).versions.set(version, plain([...keys]));
      return true;
    },
    spliceKeys(handle, version, baseVersion, start, deleteCount, inserted) {
      calls.push(
        `spliceKeys ${version} ${baseVersion} ${start} ${deleteCount} ${inserted.length}`
      );
      const store = storeOf(handle);
      const base = store.versions.get(baseVersion);
      if (!base || base.kind !== 'plain') return false;
      const keys = base.keys.slice();
      keys.splice(start, deleteCount, ...inserted);
      store.versions.set(version, plain(keys));
      return true;
    },
    setSections(
      handle,
      version,
      baseVersion,
      flags,
      sectionKeys,
      itemCounts,
      reuse,
      itemKeys
    ) {
      calls.push(`setSections ${version} ${baseVersion}`);
      const store = storeOf(handle);
      const base = store.versions.get(baseVersion);
      const header = flags % 2 === 1;
      const footer = Math.floor(flags / 2) % 2 === 1;
      const sticky = Math.floor(flags / 4) % 2 === 1;
      const keys: string[] = [];
      const stickyRows: number[] = [];
      const sectionRows: { start: number; count: number }[] = [];
      let next = 0;
      for (let section = 0; section < sectionKeys.length; section++) {
        const key = sectionKeys[section]!;
        const count =
          itemCounts[section]! + (header ? 1 : 0) + (footer ? 1 : 0);
        const start = keys.length;
        if (header && sticky) stickyRows.push(start);
        const from = reuse[section]!;
        if (from >= 0) {
          const rows = base?.sectionRows[from];
          if (!base || !rows || rows.count !== count) return false;
          if (base.sectionKeys[from] !== key) return false;
          keys.push(...base.keys.slice(rows.start, rows.start + rows.count));
        } else {
          if (header) keys.push(headerRowKey(key));
          for (let item = 0; item < itemCounts[section]!; item++) {
            keys.push(itemRowKey(key, itemKeys[next++]!));
          }
          if (footer) keys.push(footerRowKey(key));
        }
        sectionRows.push({ start, count });
      }
      store.versions.set(version, {
        keys,
        sticky: stickyRows,
        kind: 'sections',
        sectionKeys: [...sectionKeys],
        sectionRows,
        depths: [],
      });
      return true;
    },
    resetTree(handle, version, keys, depths) {
      calls.push(`resetTree ${version} ${keys.length}`);
      storeOf(handle).versions.set(version, {
        ...plain([...keys]),
        kind: 'tree',
        depths: [...depths],
      });
      return true;
    },
    expandTree(handle, version, baseVersion, row, keys, depths) {
      calls.push(`expandTree ${version} ${baseVersion} ${row} ${keys.length}`);
      const store = storeOf(handle);
      const base = store.versions.get(baseVersion);
      if (!base || base.kind !== 'tree' || row >= base.keys.length) return -1;
      const end = subtreeEnd(base.depths, row);
      const nextKeys = base.keys.slice();
      const nextDepths = base.depths.slice();
      nextKeys.splice(row + 1, end - row - 1, ...keys);
      nextDepths.splice(row + 1, end - row - 1, ...depths);
      store.versions.set(version, {
        ...plain(nextKeys),
        kind: 'tree',
        depths: nextDepths,
      });
      return end - row - 1;
    },
    collapseTree(handle, version, baseVersion, row) {
      calls.push(`collapseTree ${version} ${baseVersion} ${row}`);
      const store = storeOf(handle);
      const base = store.versions.get(baseVersion);
      if (!base || base.kind !== 'tree' || row >= base.keys.length) return -1;
      const end = subtreeEnd(base.depths, row);
      const nextKeys = base.keys.slice();
      const nextDepths = base.depths.slice();
      nextKeys.splice(row + 1, end - row - 1);
      nextDepths.splice(row + 1, end - row - 1);
      store.versions.set(version, {
        ...plain(nextKeys),
        kind: 'tree',
        depths: nextDepths,
      });
      return end - row - 1;
    },
    indexOfKey(handle, version, key) {
      calls.push(`indexOfKey ${version} ${key}`);
      const snapshot = storeOf(handle).versions.get(version);
      return snapshot ? snapshot.keys.indexOf(key) : -2;
    },
    setSizeSpecs(handle, version, keys, specs) {
      calls.push(`setSizeSpecs ${version} ${specs.length}`);
      storeOf(handle).specs.set(version, {
        keys: [...keys],
        specs: [...specs],
      });
    },
    setSelection(handle, keys, multiple) {
      calls.push(`setSelection ${keys.length} ${multiple}`);
      const store = storeOf(handle);
      store.selected = [...new Set(keys)];
      store.multiple = multiple;
    },
    selectKey(handle, key) {
      const store = storeOf(handle);
      if (!store.multiple) return selectOnly(store, key);
      if (store.selected.includes(key)) return null;
      store.selected = [...store.selected, key];
      return [...store.selected];
    },
    selectOnlyKey(handle, key) {
      return selectOnly(storeOf(handle), key);
    },
    deselectKey(handle, key) {
      const store = storeOf(handle);
      if (!store.selected.includes(key)) return null;
      store.selected = store.selected.filter((selected) => selected !== key);
      return [...store.selected];
    },
    retainSelection(handle, version) {
      calls.push(`retainSelection ${version}`);
      const store = storeOf(handle);
      const snapshot = store.versions.get(version);
      if (!snapshot) return null;
      const kept = store.selected.filter((key) => snapshot.keys.includes(key));
      if (kept.length === store.selected.length) return null;
      store.selected = kept;
      return [...kept];
    },
    selectedIndices(handle, version) {
      const snapshot = storeOf(handle).versions.get(version);
      if (!snapshot) return [];
      return storeOf(handle)
        .selected.map((key) => snapshot.keys.indexOf(key))
        .filter((index) => index >= 0)
        .sort((a, b) => a - b);
    },
  } as FakeStoreModule;
  return module;
}
