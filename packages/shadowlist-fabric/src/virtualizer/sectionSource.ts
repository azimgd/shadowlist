import type { SectionListData } from '../types';
import { defaultKeyExtractor } from './helpers';
import {
  nextVersion,
  SECTIONS_FOOTER,
  SECTIONS_HEADER,
  SECTIONS_QUALIFIED,
  SECTIONS_STICKY,
  type ListStore,
} from './nativeStore';
import { findKeyNear, lookUpKey, NO_KEYS, type RowSource } from './rowSource';
import { footerRowKey, headerRowKey, itemRowKey } from './sectionKeys';

type FlatRowType = 'sectionHeader' | 'item' | 'sectionFooter';

/*
 * One row SectionList renders. Built only for the rows asked about, which are the mounted ones.
 */
export interface FlatRow<ItemT, SectionT> {
  key: string;
  type: FlatRowType;
  sectionIndex: number;
  section?: SectionListData<ItemT, SectionT>;
  item?: ItemT;
  itemIndex?: number;
  rowKey?: string;
  isLastInSection?: boolean;
  isSectionBoundary?: boolean;
}

/*
 * Whether the row built for this position matches the mounted one. Compare everything the
 * row renders from, or a reused row would show stale content.
 */
function sameRow<ItemT, SectionT>(
  previous: FlatRow<ItemT, SectionT> | undefined,
  next: FlatRow<ItemT, SectionT>
): previous is FlatRow<ItemT, SectionT> {
  return (
    previous !== undefined &&
    previous.type === next.type &&
    previous.sectionIndex === next.sectionIndex &&
    previous.section === next.section &&
    previous.item === next.item &&
    previous.itemIndex === next.itemIndex &&
    previous.rowKey === next.rowKey &&
    previous.isLastInSection === next.isLastInSection &&
    previous.isSectionBoundary === next.isSectionBoundary
  );
}

/*
 * One section as last written: its items when they were keyed, to notice an array edited in
 * place, and their keys.
 */
interface WrittenSection<ItemT> {
  key: string;
  items: ItemT[];
  keyExtractor: ((item: ItemT, index: number) => string) | undefined;
  itemKeys: string[];
  position: number;
}

export interface SectionShape {
  hasHeader: boolean;
  hasFooter: boolean;
  stickyEnabled: boolean;
}

/*
 * The rows of a SectionList: each section's header, items and footer. JS keys each section's
 * items and native builds the row keys and the sticky header rows from them, reusing the rows
 * of sections that came back unchanged. The rows themselves are built only when asked for.
 * firstRowOfSection gives a section's first row, its header when it has one, or -1 for a
 * section without rows. rowKeysOfItems gives the row keys of items by item key, in row order,
 * for props that name items.
 */
export interface SectionSource<ItemT, SectionT> extends RowSource<
  FlatRow<ItemT, SectionT>
> {
  readonly sections: ReadonlyArray<SectionListData<ItemT, SectionT>>;
  readonly shape: SectionShape;
  firstRowOfSection(sectionIndex: number): number;
  rowKeysOfItems(itemKeys: ReadonlyArray<string>): string[];
}

/*
 * What a SectionList keeps between flattens.
 */
export interface SectionSourceState<ItemT, SectionT> {
  written: WrittenSection<ItemT>[];
  byKey: Map<string, WrittenSection<ItemT>>;
  shape: SectionShape | null;
  source: SectionSource<ItemT, SectionT> | null;
  sections: ReadonlyArray<SectionListData<ItemT, SectionT>> | null;
  keyExtractor: ((item: ItemT, index: number) => string) | undefined;
}

export function createSectionSourceState<ItemT, SectionT>(): SectionSourceState<
  ItemT,
  SectionT
> {
  return {
    written: [],
    byKey: new Map(),
    shape: null,
    source: null,
    sections: null,
    keyExtractor: undefined,
  };
}

function sameItems<ItemT>(
  written: WrittenSection<ItemT> | undefined,
  items: ReadonlyArray<ItemT>,
  keyExtractor: ((item: ItemT, index: number) => string) | undefined
): written is WrittenSection<ItemT> {
  if (
    written === undefined ||
    written.keyExtractor !== keyExtractor ||
    written.items.length !== items.length
  ) {
    return false;
  }
  for (let index = 0; index < items.length; index++) {
    if (written.items[index] !== items[index]) return false;
  }
  return true;
}

function sameShape(a: SectionShape | null, b: SectionShape): boolean {
  return (
    a !== null &&
    a.hasHeader === b.hasHeader &&
    a.hasFooter === b.hasFooter &&
    a.stickyEnabled === b.stickyEnabled
  );
}

/*
 * The source for sections, writing their keys to the store. Unchanged sections keep their
 * item keys without calling keyExtractor and native copies their rows. Rows keep their objects
 * across sources while nothing they render from changed.
 */
export function buildSectionSource<ItemT, SectionT>(
  state: SectionSourceState<ItemT, SectionT>,
  sections: ReadonlyArray<SectionListData<ItemT, SectionT>>,
  keyExtractor: ((item: ItemT, index: number) => string) | undefined,
  shape: SectionShape,
  store: ListStore | null
): SectionSource<ItemT, SectionT> {
  // The same inputs again, like a Strict Mode second run, give the same source.
  if (
    state.source !== null &&
    state.sections === sections &&
    state.keyExtractor === keyExtractor &&
    sameShape(state.shape, shape)
  ) {
    return state.source;
  }
  const canReuse = sameShape(state.shape, shape);
  const count = sections.length;
  const written: WrittenSection<ItemT>[] = new Array(count);
  const byKey = new Map<string, WrittenSection<ItemT>>();
  const sectionKeys = new Array<string>(count);
  const itemCounts = new Array<number>(count);
  const reuse = new Array<number>(count);
  const newItemKeys: string[] = [];
  const rowStarts = new Array<number>(count + 1);
  const edgeRows = (shape.hasHeader ? 1 : 0) + (shape.hasFooter ? 1 : 0);
  let rows = 0;

  for (let sectionIndex = 0; sectionIndex < count; sectionIndex++) {
    const section = sections[sectionIndex]!;
    const key = section.key ?? `section-${sectionIndex}`;
    const sectionKeyExtractor = section.keyExtractor ?? keyExtractor;
    const items = section.data;
    const previous = state.byKey.get(key);
    let entry: WrittenSection<ItemT>;
    if (sameItems(previous, items, sectionKeyExtractor) && !byKey.has(key)) {
      entry = { ...previous, position: sectionIndex };
      reuse[sectionIndex] = canReuse ? previous.position : -1;
    } else {
      const itemKeys = new Array<string>(items.length);
      for (let index = 0; index < items.length; index++) {
        itemKeys[index] = sectionKeyExtractor
          ? sectionKeyExtractor(items[index]!, index)
          : defaultKeyExtractor(items[index], index);
      }
      entry = {
        key,
        items: items.slice(),
        keyExtractor: sectionKeyExtractor,
        itemKeys,
        position: sectionIndex,
      };
      reuse[sectionIndex] = -1;
    }
    if (reuse[sectionIndex]! < 0) {
      for (const itemKey of entry.itemKeys) newItemKeys.push(itemKey);
    }
    written[sectionIndex] = entry;
    if (!byKey.has(key)) byKey.set(key, entry);
    sectionKeys[sectionIndex] = key;
    itemCounts[sectionIndex] = items.length;
    rowStarts[sectionIndex] = rows;
    rows += items.length + edgeRows;
  }
  rowStarts[count] = rows;

  let version = 0;
  if (store !== null) {
    version = nextVersion(store);
    const flags =
      SECTIONS_QUALIFIED +
      (shape.hasHeader ? SECTIONS_HEADER : 0) +
      (shape.hasFooter ? SECTIONS_FOOTER : 0) +
      (shape.stickyEnabled ? SECTIONS_STICKY : 0);
    const baseVersion = canReuse ? (state.source?.keysVersion ?? 0) : 0;
    const writtenNatively = store.module.setSections(
      store.handle,
      version,
      baseVersion,
      flags,
      sectionKeys,
      itemCounts,
      reuse,
      newItemKeys
    );
    if (!writtenNatively) {
      // The base is gone: write every section's items.
      const allItemKeys: string[] = [];
      for (const entry of written) {
        for (const itemKey of entry.itemKeys) allItemKeys.push(itemKey);
      }
      store.module.setSections(
        store.handle,
        version,
        0,
        flags,
        sectionKeys,
        itemCounts,
        reuse.map(() => -1),
        allItemKeys
      );
    }
  }

  const source = sectionSource(
    sections,
    written,
    rowStarts,
    shape,
    store,
    version,
    state.source
  );
  state.written = written;
  state.byKey = byKey;
  state.shape = shape;
  state.source = source;
  state.sections = sections;
  state.keyExtractor = keyExtractor;
  return source;
}

interface RowPlace {
  sectionIndex: number;
  type: FlatRowType;
  itemIndex: number;
}

function sectionSource<ItemT, SectionT>(
  sections: ReadonlyArray<SectionListData<ItemT, SectionT>>,
  written: ReadonlyArray<WrittenSection<ItemT>>,
  rowStarts: ReadonlyArray<number>,
  shape: SectionShape,
  store: ListStore | null,
  version: number,
  previous: SectionSource<ItemT, SectionT> | null
): SectionSource<ItemT, SectionT> {
  const sectionCount = sections.length;
  const count = rowStarts[sectionCount]!;
  const { hasHeader, hasFooter } = shape;

  /*
   * The section a row is in: the last section starting at or before it. Sections without rows
   * share their start with the next one, and the search lands on the last of them.
   */
  const placeOf = (row: number): RowPlace | null => {
    if (row < 0 || row >= count) return null;
    let low = 0;
    let high = sectionCount - 1;
    while (low < high) {
      const middle = Math.ceil((low + high) / 2);
      if (rowStarts[middle]! <= row) low = middle;
      else high = middle - 1;
    }
    let local = row - rowStarts[low]!;
    if (hasHeader) {
      if (local === 0)
        return { sectionIndex: low, type: 'sectionHeader', itemIndex: -1 };
      local--;
    }
    if (local < written[low]!.itemKeys.length) {
      return { sectionIndex: low, type: 'item', itemIndex: local };
    }
    return { sectionIndex: low, type: 'sectionFooter', itemIndex: -1 };
  };

  const keyAt = (row: number): string | undefined => {
    const place = placeOf(row);
    if (place === null) return undefined;
    const entry = written[place.sectionIndex]!;
    if (place.type === 'sectionHeader') return headerRowKey(entry.key);
    if (place.type === 'sectionFooter') return footerRowKey(entry.key);
    return itemRowKey(entry.key, entry.itemKeys[place.itemIndex]!);
  };

  // Every row's key, only built without the store or when the store lost the version.
  let allKeys: string[] | null = null;
  const rowKeys = (): string[] => {
    if (allKeys === null) {
      allKeys = new Array<string>(count);
      let row = 0;
      for (const entry of written) {
        if (hasHeader) allKeys[row++] = headerRowKey(entry.key);
        for (const itemKey of entry.itemKeys) {
          allKeys[row++] = itemRowKey(entry.key, itemKey);
        }
        if (hasFooter) allKeys[row++] = footerRowKey(entry.key);
      }
    }
    return allKeys;
  };

  /*
   * Rows built for this source, by key. A row asked for again, here or by the next source,
   * keeps its object while it would come out the same.
   */
  const builtRows = new Map<string, FlatRow<ItemT, SectionT>>();
  // Only the previous source's rows, not the source. Older sources can then go.
  const previousRows =
    (previous as SectionSourceWithRows<ItemT, SectionT> | null)?.builtRows ??
    null;

  const itemAt = (row: number): FlatRow<ItemT, SectionT> | undefined => {
    const place = placeOf(row);
    if (place === null) return undefined;
    const key = keyAt(row)!;
    const existing = builtRows.get(key);
    if (existing !== undefined) return existing;
    const { sectionIndex } = place;
    const section = sections[sectionIndex]!;
    const isLastSection = sectionIndex === sectionCount - 1;
    let next: FlatRow<ItemT, SectionT>;
    if (place.type === 'sectionHeader') {
      next = { key, type: 'sectionHeader', section, sectionIndex };
    } else if (place.type === 'sectionFooter') {
      next = {
        key,
        type: 'sectionFooter',
        section,
        sectionIndex,
        isSectionBoundary: !isLastSection,
      };
    } else {
      const entry = written[sectionIndex]!;
      const isLastInSection = place.itemIndex === entry.itemKeys.length - 1;
      next = {
        key,
        type: 'item',
        sectionIndex,
        item: section.data[place.itemIndex],
        itemIndex: place.itemIndex,
        rowKey: entry.itemKeys[place.itemIndex],
        isLastInSection,
        isSectionBoundary: isLastInSection && !hasFooter && !isLastSection,
      };
    }
    const kept = previousRows?.get(key);
    const result = sameRow(kept, next) ? kept : next;
    builtRows.set(key, result);
    return result;
  };

  const source: SectionSourceWithRows<ItemT, SectionT> = {
    count,
    itemAt,
    keyAt,
    indexOfKey(key, hint) {
      const near = findKeyNear(key, hint, count, keyAt);
      if (near >= 0) return near;
      return lookUpKey(store, version, key, rowKeys);
    },
    indexOfItem(item) {
      for (let sectionIndex = 0; sectionIndex < sectionCount; sectionIndex++) {
        const index = sections[sectionIndex]!.data.indexOf(item as ItemT);
        if (index >= 0) {
          return rowStarts[sectionIndex]! + (hasHeader ? 1 : 0) + index;
        }
      }
      return -1;
    },
    data: null,
    keysVersion: version,
    propKeys: store === null ? rowKeys() : NO_KEYS,
    nativeSticky:
      store !== null && hasHeader && shape.stickyEnabled && sectionCount > 0,
    stickyIndices:
      store === null && hasHeader && shape.stickyEnabled
        ? rowStarts.slice(0, sectionCount)
        : null,
    sections,
    shape,
    firstRowOfSection(sectionIndex) {
      if (sectionIndex < 0 || sectionIndex >= sectionCount) return -1;
      const start = rowStarts[sectionIndex]!;
      return rowStarts[sectionIndex + 1]! > start ? start : -1;
    },
    rowKeysOfItems(itemKeys) {
      if (itemKeys.length === 0) return [];
      const wanted = new Set(itemKeys);
      const result: string[] = [];
      for (const entry of written) {
        for (const itemKey of entry.itemKeys) {
          if (wanted.has(itemKey)) result.push(itemRowKey(entry.key, itemKey));
        }
      }
      return result;
    },
    builtRows,
  };
  return source;
}

interface SectionSourceWithRows<ItemT, SectionT> extends SectionSource<
  ItemT,
  SectionT
> {
  readonly builtRows: Map<string, FlatRow<ItemT, SectionT>>;
}

/*
 * Every row of a source, for tests.
 */
export function rowsOf<ItemT>(source: RowSource<ItemT>): ItemT[] {
  const rows: ItemT[] = [];
  for (let row = 0; row < source.count; row++) rows.push(source.itemAt(row)!);
  return rows;
}
