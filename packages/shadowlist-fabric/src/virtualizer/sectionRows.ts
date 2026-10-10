import type { SectionListData } from '../types';
import { defaultKeyExtractor } from './helpers';

type FlatRowType = 'sectionHeader' | 'item' | 'sectionFooter';

export interface FlatRow<ItemT, SectionT> {
  id: string;
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
 * One section's rows from the last flatten, with everything they were built from. A section
 * that comes back unchanged reuses them without building a single row or id string.
 */
export interface SectionRows<ItemT, SectionT> {
  section: SectionListData<ItemT, SectionT>;
  sectionIndex: number;
  keyExtractor: ((item: ItemT, index: number) => string) | undefined;
  hasHeader: boolean;
  hasFooter: boolean;
  isLastSection: boolean;
  rows: FlatRow<ItemT, SectionT>[];
  itemStart: number;
}

/*
 * Whether cached rows still describe the section. The items are compared one by one, not
 * just the data array. A data array edited in place is still picked up.
 */
function sectionUnchanged<ItemT, SectionT>(
  cached: SectionRows<ItemT, SectionT> | undefined,
  section: SectionListData<ItemT, SectionT>,
  sectionIndex: number,
  keyExtractor: ((item: ItemT, index: number) => string) | undefined,
  hasHeader: boolean,
  hasFooter: boolean,
  isLastSection: boolean
): boolean {
  if (
    cached === undefined ||
    cached.section !== section ||
    cached.sectionIndex !== sectionIndex ||
    cached.keyExtractor !== keyExtractor ||
    cached.hasHeader !== hasHeader ||
    cached.hasFooter !== hasFooter ||
    cached.isLastSection !== isLastSection
  ) {
    return false;
  }
  const items = section.data;
  const itemCount = cached.rows.length - cached.itemStart - (hasFooter ? 1 : 0);
  if (items.length !== itemCount) return false;
  for (let index = 0; index < itemCount; index++) {
    if (cached.rows[cached.itemStart + index]!.item !== items[index]) {
      return false;
    }
  }
  return true;
}

/*
 * Sections to flat rows plus the sticky header positions. previousSections is the last
 * result's nextSections. Unchanged sections and rows keep their objects.
 */
export function flattenSections<ItemT, SectionT>(
  sections: ReadonlyArray<SectionListData<ItemT, SectionT>>,
  keyExtractor: ((item: ItemT, index: number) => string) | undefined,
  hasHeader: boolean,
  hasFooter: boolean,
  stickyEnabled: boolean,
  previousSections: ReadonlyMap<string, SectionRows<ItemT, SectionT>>
): {
  rows: FlatRow<ItemT, SectionT>[];
  stickyIndices: number[];
  nextSections: Map<string, SectionRows<ItemT, SectionT>>;
} {
  const rows: FlatRow<ItemT, SectionT>[] = [];
  const stickyIndices: number[] = [];
  const nextSections = new Map<string, SectionRows<ItemT, SectionT>>();

  sections.forEach((section, sectionIndex) => {
    const sectionKey = section.key ?? `section-${sectionIndex}`;
    const sectionKeyExtractor = section.keyExtractor ?? keyExtractor;
    const isLastSection = sectionIndex === sections.length - 1;
    const cached = previousSections.get(sectionKey);

    if (hasHeader && stickyEnabled) {
      stickyIndices.push(rows.length);
    }

    if (
      sectionUnchanged(
        cached,
        section,
        sectionIndex,
        sectionKeyExtractor,
        hasHeader,
        hasFooter,
        isLastSection
      )
    ) {
      for (const row of cached!.rows) rows.push(row);
      nextSections.set(sectionKey, cached!);
      return;
    }

    // Built lazily, only for a section that changed.
    let previousRows: Map<string, FlatRow<ItemT, SectionT>> | null = null;
    const sectionRows: FlatRow<ItemT, SectionT>[] = [];
    const push = (row: FlatRow<ItemT, SectionT>) => {
      if (previousRows === null) {
        previousRows = new Map();
        for (const previousRow of cached?.rows ?? []) {
          previousRows.set(previousRow.id, previousRow);
        }
      }
      const previousRow = previousRows.get(row.id);
      const finalRow = sameRow(previousRow, row) ? previousRow : row;
      sectionRows.push(finalRow);
      rows.push(finalRow);
    };

    if (hasHeader) {
      push({
        id: `sh:${sectionKey}`,
        type: 'sectionHeader',
        section,
        sectionIndex,
      });
    }

    const itemStart = sectionRows.length;
    const lastItemIndex = section.data.length - 1;
    section.data.forEach((item, itemIndex) => {
      const rowKey = sectionKeyExtractor
        ? sectionKeyExtractor(item, itemIndex)
        : defaultKeyExtractor(item, itemIndex);
      const isLastInSection = itemIndex === lastItemIndex;
      push({
        id: `si:${sectionKey}:${rowKey}`,
        type: 'item',
        sectionIndex,
        item,
        itemIndex,
        rowKey,
        isLastInSection,
        isSectionBoundary: isLastInSection && !hasFooter && !isLastSection,
      });
    });

    if (hasFooter) {
      push({
        id: `sf:${sectionKey}`,
        type: 'sectionFooter',
        section,
        sectionIndex,
        isSectionBoundary: !isLastSection,
      });
    }

    nextSections.set(sectionKey, {
      section,
      sectionIndex,
      keyExtractor: sectionKeyExtractor,
      hasHeader,
      hasFooter,
      isLastSection,
      rows: sectionRows,
      itemStart,
    });
  });

  return { rows, stickyIndices, nextSections };
}
