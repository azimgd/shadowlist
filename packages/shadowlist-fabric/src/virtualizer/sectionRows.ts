import type { SectionListData } from '../types';

type FlatRowType = 'sectionHeader' | 'element' | 'sectionFooter';

export interface FlatRow<ElementT, SectionT> {
  id: string;
  type: FlatRowType;
  sectionIndex: number;
  section?: SectionListData<ElementT, SectionT>;
  element?: ElementT;
  elementIndex?: number;
  elementKey?: string;
  isLastInSection?: boolean;
  isSectionBoundary?: boolean;
}

/*
 * Whether the row built for this position matches the mounted one. Compare everything the
 * row renders from, or a reused row would show old content.
 */
function sameRow<ElementT, SectionT>(
  previous: FlatRow<ElementT, SectionT> | undefined,
  next: FlatRow<ElementT, SectionT>
): previous is FlatRow<ElementT, SectionT> {
  return (
    previous !== undefined &&
    previous.type === next.type &&
    previous.sectionIndex === next.sectionIndex &&
    previous.section === next.section &&
    previous.element === next.element &&
    previous.elementIndex === next.elementIndex &&
    previous.elementKey === next.elementKey &&
    previous.isLastInSection === next.isLastInSection &&
    previous.isSectionBoundary === next.isSectionBoundary
  );
}

/*
 * One section's rows from the last flatten, with everything they were built from. A section
 * that comes back unchanged reuses them without building a single row or id string.
 */
export interface SectionRows<ElementT, SectionT> {
  section: SectionListData<ElementT, SectionT>;
  sectionIndex: number;
  keyExtractor: ((element: ElementT, index: number) => string) | undefined;
  hasHeader: boolean;
  hasFooter: boolean;
  isLastSection: boolean;
  rows: FlatRow<ElementT, SectionT>[];
  // Where the element rows start in rows.
  elementStart: number;
}

/*
 * Whether cached rows still describe the section. The elements are compared one by one, not
 * just the data array, so a data array edited in place is still picked up.
 */
function sectionUnchanged<ElementT, SectionT>(
  cached: SectionRows<ElementT, SectionT> | undefined,
  section: SectionListData<ElementT, SectionT>,
  sectionIndex: number,
  keyExtractor: ((element: ElementT, index: number) => string) | undefined,
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
  const elements = section.data;
  const elementCount =
    cached.rows.length - cached.elementStart - (hasFooter ? 1 : 0);
  if (elements.length !== elementCount) return false;
  for (let index = 0; index < elementCount; index++) {
    if (cached.rows[cached.elementStart + index]!.element !== elements[index]) {
      return false;
    }
  }
  return true;
}

/*
 * Sections to flat rows plus the sticky header positions. previousSections is the last
 * result's nextSections, so unchanged sections and rows keep their objects.
 */
export function flattenSections<ElementT, SectionT>(
  sections: ReadonlyArray<SectionListData<ElementT, SectionT>>,
  keyExtractor: ((element: ElementT, index: number) => string) | undefined,
  hasHeader: boolean,
  hasFooter: boolean,
  stickyEnabled: boolean,
  previousSections: ReadonlyMap<string, SectionRows<ElementT, SectionT>>
): {
  rows: FlatRow<ElementT, SectionT>[];
  stickyIndices: number[];
  nextSections: Map<string, SectionRows<ElementT, SectionT>>;
} {
  const rows: FlatRow<ElementT, SectionT>[] = [];
  const stickyIndices: number[] = [];
  const nextSections = new Map<string, SectionRows<ElementT, SectionT>>();

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
    let previousRows: Map<string, FlatRow<ElementT, SectionT>> | null = null;
    const sectionRows: FlatRow<ElementT, SectionT>[] = [];
    const push = (row: FlatRow<ElementT, SectionT>) => {
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

    const elementStart = sectionRows.length;
    const lastElementIndex = section.data.length - 1;
    section.data.forEach((element, elementIndex) => {
      const elementKey = sectionKeyExtractor
        ? sectionKeyExtractor(element, elementIndex)
        : ((element as { id?: string })?.id ?? `${elementIndex}`);
      const isLastInSection = elementIndex === lastElementIndex;
      push({
        id: `si:${sectionKey}:${elementKey}`,
        type: 'element',
        sectionIndex,
        element,
        elementIndex,
        elementKey,
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
      elementStart,
    });
  });

  return { rows, stickyIndices, nextSections };
}
