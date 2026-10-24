import {
  nextVersion,
  TREE_EXPANDED,
  TREE_HAS_CHILDREN,
  type ListStore,
} from './nativeStore';
import { findKeyNear, lookUpKey, NO_KEYS, type RowSource } from './rowSource';

/*
 * One row TreeList renders. Built only for the rows asked about, which are the mounted ones.
 */
export interface TreeFlatRow<ItemT> {
  key: string;
  item: ItemT;
  depth: number;
  hasChildren: boolean;
  isExpanded: boolean;
}

/*
 * The visible rows depth first: each row's node, and its depth and flags packed into one
 * number, depth * META_DEPTH plus TREE_HAS_CHILDREN and TREE_EXPANDED. Keys are not kept: a
 * row's key is keyExtractor of its node, and native holds every key. Two arrays because a
 * toggle copies them, and a copy of 100k entries costs about a millisecond in Hermes. A new
 * pair per change. An older source keeps its rows.
 */
interface TreeMirror<ItemT> {
  items: ItemT[];
  meta: number[];
}

/*
 * A walk's rows and their keys, which native takes.
 */
interface WalkedRows<ItemT> extends TreeMirror<ItemT> {
  keys: string[];
}

const META_DEPTH = 4;

function depthOf(meta: number): number {
  return Math.floor(meta / META_DEPTH);
}

function hasChildrenOf(meta: number): boolean {
  return meta % 2 === 1;
}

function isExpandedOf(meta: number): boolean {
  return meta % META_DEPTH >= TREE_EXPANDED;
}

function nativeRows(meta: ReadonlyArray<number>): {
  depths: number[];
  flags: number[];
} {
  const depths = new Array<number>(meta.length);
  const flags = new Array<number>(meta.length);
  for (let row = 0; row < meta.length; row++) {
    depths[row] = depthOf(meta[row]!);
    flags[row] = meta[row]! % META_DEPTH;
  }
  return { depths, flags };
}

export interface TreeSourceState<ItemT> {
  data: ReadonlyArray<ItemT> | null;
  getChildren: ((item: ItemT) => ReadonlyArray<ItemT> | undefined) | null;
  keyExtractor: ((item: ItemT) => string) | null;
  expanded: ReadonlySet<string>;
  mirror: TreeMirror<ItemT> | null;
  source: TreeSourceWithRows<ItemT> | null;
}

export function createTreeSourceState<ItemT>(): TreeSourceState<ItemT> {
  return {
    data: null,
    getChildren: null,
    keyExtractor: null,
    expanded: new Set(),
    mirror: null,
    source: null,
  };
}

/*
 * More expanded keys changing at once than this rebuilds the rows instead of splicing.
 */
const MAX_SPLICED_TOGGLES = 32;

/*
 * The rows of nodes, depth first from depth, skipping the children of collapsed nodes.
 */
function walk<ItemT>(
  nodes: ReadonlyArray<ItemT>,
  depth: number,
  getChildren: (item: ItemT) => ReadonlyArray<ItemT> | undefined,
  keyExtractor: (item: ItemT) => string,
  expanded: ReadonlySet<string>
): WalkedRows<ItemT> {
  const rows: WalkedRows<ItemT> = { items: [], meta: [], keys: [] };
  const stackItems: ItemT[] = [];
  const stackDepths: number[] = [];
  for (let index = nodes.length - 1; index >= 0; index--) {
    stackItems.push(nodes[index]!);
    stackDepths.push(depth);
  }
  while (stackItems.length > 0) {
    const item = stackItems.pop()!;
    const itemDepth = stackDepths.pop()!;
    const key = keyExtractor(item);
    const children = getChildren(item);
    const hasChildren = !!children && children.length > 0;
    const isExpanded = hasChildren && expanded.has(key);
    rows.items.push(item);
    rows.keys.push(key);
    rows.meta.push(
      itemDepth * META_DEPTH +
        (hasChildren ? TREE_HAS_CHILDREN : 0) +
        (isExpanded ? TREE_EXPANDED : 0)
    );
    if (isExpanded) {
      for (let index = children!.length - 1; index >= 0; index--) {
        stackItems.push(children![index]!);
        stackDepths.push(itemDepth + 1);
      }
    }
  }
  return rows;
}

function splice<T>(
  values: ReadonlyArray<T>,
  start: number,
  deleted: number,
  inserted: ReadonlyArray<T>
): T[] {
  return values.slice(0, start).concat(inserted, values.slice(start + deleted));
}

/*
 * The row after a row's visible descendants.
 */
function subtreeEnd(meta: ReadonlyArray<number>, row: number): number {
  const depth = depthOf(meta[row]!);
  let end = row + 1;
  while (end < meta.length && depthOf(meta[end]!) > depth) end++;
  return end;
}

function changedKeys(
  previous: ReadonlySet<string>,
  next: ReadonlySet<string>
): string[] | null {
  const changed: string[] = [];
  for (const key of next) {
    if (!previous.has(key)) {
      changed.push(key);
      if (changed.length > MAX_SPLICED_TOGGLES) return null;
    }
  }
  for (const key of previous) {
    if (!next.has(key)) {
      changed.push(key);
      if (changed.length > MAX_SPLICED_TOGGLES) return null;
    }
  }
  return changed;
}

/*
 * One expand or collapse the caller made: key toggled in from, which gave the new set. It
 * spares comparing every expanded key of both sets.
 */
export interface TreeToggle {
  from: ReadonlySet<string>;
  to: ReadonlySet<string>;
  key: string;
}

/*
 * The source for a tree. New data rebuilds the rows. An expand or collapse with the same data
 * splices only the toggled node's descendants, here and in the native store, which owns the
 * row keys and checks each splice against its own rows. A toggled row is found where it was
 * last built, which for a tap is a mounted row.
 */
export function buildTreeSource<ItemT>(
  state: TreeSourceState<ItemT>,
  data: ReadonlyArray<ItemT>,
  getChildren: (item: ItemT) => ReadonlyArray<ItemT> | undefined,
  keyExtractor: (item: ItemT) => string,
  expanded: ReadonlySet<string>,
  store: ListStore | null,
  toggle: TreeToggle | null = null
): RowSource<TreeFlatRow<ItemT>> {
  const sameData =
    state.mirror !== null &&
    state.data === data &&
    state.getChildren === getChildren &&
    state.keyExtractor === keyExtractor;
  if (sameData && state.expanded === expanded && state.source !== null) {
    return state.source;
  }
  let mirror: TreeMirror<ItemT>;
  let keys: string[] | null = null;
  let version = state.source?.keysVersion ?? 0;
  let toggles: string[] | null = null;
  if (sameData) {
    toggles =
      toggle !== null &&
      toggle.from === state.expanded &&
      toggle.to === expanded
        ? [toggle.key]
        : changedKeys(state.expanded, expanded);
  }

  if (toggles === null) {
    const walked = walk(data, 0, getChildren, keyExtractor, expanded);
    mirror = walked;
    keys = walked.keys;
    if (store !== null) {
      version = nextVersion(store);
      const { depths, flags } = nativeRows(walked.meta);
      store.module.resetTree(store.handle, version, keys, depths, flags);
    }
  } else {
    mirror = state.mirror!;
    const previous = state.source!;
    let current: TreeSourceWithRows<ItemT> = previous;
    let inStep = store !== null;
    for (const key of toggles) {
      const row = current.indexOfKey(key, previous.builtIndex.get(key));
      if (row < 0) continue;
      const meta = mirror.meta[row]!;
      if (!hasChildrenOf(meta)) continue;
      const shouldExpand = expanded.has(key);
      if (shouldExpand === isExpandedOf(meta)) continue;
      const end = subtreeEnd(mirror.meta, row);
      const shown = shouldExpand
        ? walk(
            getChildren(mirror.items[row]!) ?? [],
            depthOf(meta) + 1,
            getChildren,
            keyExtractor,
            expanded
          )
        : null;
      const deleted = end - row - 1;
      const nextMeta = splice(mirror.meta, row + 1, deleted, shown?.meta ?? []);
      nextMeta[row] = meta + (shouldExpand ? TREE_EXPANDED : -TREE_EXPANDED);
      mirror = {
        items: splice(mirror.items, row + 1, deleted, shown?.items ?? []),
        meta: nextMeta,
      };
      if (store !== null && inStep) {
        const base = version;
        version = nextVersion(store);
        let nativeDeleted: number;
        if (shown) {
          const { depths, flags } = nativeRows(shown.meta);
          nativeDeleted = store.module.expandTree(
            store.handle,
            version,
            base,
            row,
            shown.keys,
            depths,
            flags
          );
        } else {
          nativeDeleted = store.module.collapseTree(
            store.handle,
            version,
            base,
            row
          );
        }
        inStep = nativeDeleted === deleted;
      }
      current = treeSource(mirror, keyExtractor, null, store, version, null);
    }
    // Native lost the base or disagreed: give it the rows as JS has them.
    if (store !== null && !inStep) {
      version = nextVersion(store);
      const { depths, flags } = nativeRows(mirror.meta);
      store.module.resetTree(
        store.handle,
        version,
        mirror.items.map((item) => keyExtractor(item)),
        depths,
        flags
      );
    }
  }

  const source = treeSource(
    mirror,
    keyExtractor,
    keys,
    store,
    version,
    state.source
  );
  state.data = data;
  state.getChildren = getChildren;
  state.keyExtractor = keyExtractor;
  state.expanded = expanded;
  state.mirror = mirror;
  state.source = source;
  return source;
}

/*
 * A source with the rows it built and where each was, the hint for a toggle of it.
 */
interface TreeSourceWithRows<ItemT> extends RowSource<TreeFlatRow<ItemT>> {
  readonly builtRows: Map<string, TreeFlatRow<ItemT>>;
  readonly builtIndex: Map<string, number>;
}

function treeSource<ItemT>(
  mirror: TreeMirror<ItemT>,
  keyExtractor: (item: ItemT) => string,
  knownKeys: string[] | null,
  store: ListStore | null,
  version: number,
  previous: TreeSourceWithRows<ItemT> | null
): TreeSourceWithRows<ItemT> {
  const { items, meta } = mirror;
  const count = items.length;
  // Every key, only built without the store or when the store cannot answer.
  let keys = knownKeys;
  const allKeys = (): string[] => {
    if (keys === null) keys = items.map((item) => keyExtractor(item));
    return keys;
  };
  const keyAt = (row: number) =>
    row >= 0 && row < count
      ? (keys?.[row] ?? keyExtractor(items[row]!))
      : undefined;
  const builtRows = new Map<string, TreeFlatRow<ItemT>>();
  const builtIndex = new Map<string, number>();
  const previousRows = previous?.builtRows ?? null;
  const source: TreeSourceWithRows<ItemT> = {
    count,
    itemAt(row) {
      if (row < 0 || row >= count) return undefined;
      const key = keyAt(row)!;
      const existing = builtRows.get(key);
      if (existing !== undefined) return existing;
      const rowMeta = meta[row]!;
      const next: TreeFlatRow<ItemT> = {
        key,
        item: items[row]!,
        depth: depthOf(rowMeta),
        hasChildren: hasChildrenOf(rowMeta),
        isExpanded: isExpandedOf(rowMeta),
      };
      const kept = previousRows?.get(key);
      const result =
        kept !== undefined &&
        kept.item === next.item &&
        kept.depth === next.depth &&
        kept.hasChildren === next.hasChildren &&
        kept.isExpanded === next.isExpanded
          ? kept
          : next;
      builtRows.set(key, result);
      builtIndex.set(key, row);
      return result;
    },
    keyAt,
    indexOfKey(key, hint) {
      const near = findKeyNear(key, hint, count, keyAt);
      if (near >= 0) return near;
      return lookUpKey(store, version, key, allKeys);
    },
    indexOfItem: (item) => items.indexOf(item as ItemT),
    data: null,
    keysVersion: version,
    get propKeys() {
      return store === null ? allKeys() : NO_KEYS;
    },
    nativeSticky: false,
    stickyIndices: null,
    builtRows,
    builtIndex,
  };
  return source;
}
