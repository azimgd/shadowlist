import type { SectionListData } from './types';

/*
 * Flattens `sections` into one tagged element stream (section header, items, section
 * footer) for Shadowlist. Section headers carry their flat indices in
 * `stickyHeaderIndices` for native pinning. Each row also carries which separator
 * renders after its content, so the renderer is a pure dispatch.
 */

export type SectionFlatRowType = 'sectionHeader' | 'item' | 'sectionFooter';

export type SectionFlatRowSeparator = 'item' | 'section' | null;

export interface SectionFlatRow<ItemT, SectionT> {
  /* Stable key for the flattened row, derived from section key and item key. */
  id: string;
  type: SectionFlatRowType;
  section: SectionListData<ItemT, SectionT>;
  sectionIndex: number;
  /* Item payload (item rows only) and its index within the section. */
  item?: ItemT;
  itemIndex?: number;
  /*
   * Separator after this row's content: 'item' between items within a section,
   * 'section' on the last row of a non-final section (the footer when one renders,
   * otherwise the last item), null elsewhere.
   */
  separator: SectionFlatRowSeparator;
}

export interface FlattenSectionsInput<ItemT, SectionT> {
  sections: ReadonlyArray<SectionListData<ItemT, SectionT>>;
  keyExtractor?: (item: ItemT, index: number) => string;
  /* Whether header/footer rows render at all (a renderer was provided). */
  hasSectionHeader: boolean;
  hasSectionFooter: boolean;
  stickySectionHeadersEnabled: boolean;
}

export interface FlattenSectionsResult<ItemT, SectionT> {
  rows: SectionFlatRow<ItemT, SectionT>[];
  stickyHeaderIndices: number[];
}

export const flattenSections = <ItemT, SectionT = object>({
  sections,
  keyExtractor,
  hasSectionHeader,
  hasSectionFooter,
  stickySectionHeadersEnabled,
}: FlattenSectionsInput<ItemT, SectionT>): FlattenSectionsResult<
  ItemT,
  SectionT
> => {
  const rows: SectionFlatRow<ItemT, SectionT>[] = [];
  const stickyHeaderIndices: number[] = [];

  sections.forEach((section, sectionIndex) => {
    const sectionKey = section.key ?? `section-${sectionIndex}`;
    const sectionKeyExtractor = section.keyExtractor ?? keyExtractor;
    const isLastSection = sectionIndex === sections.length - 1;

    if (hasSectionHeader) {
      if (stickySectionHeadersEnabled) {
        stickyHeaderIndices.push(rows.length);
      }
      rows.push({
        id: `sh:${sectionKey}`,
        type: 'sectionHeader',
        section,
        sectionIndex,
        separator: null,
      });
    }

    const lastItemIndex = section.data.length - 1;
    section.data.forEach((item, itemIndex) => {
      const itemKey = sectionKeyExtractor
        ? sectionKeyExtractor(item, itemIndex)
        : ((item as { id?: string })?.id ?? `${itemIndex}`);
      const isLastInSection = itemIndex === lastItemIndex;
      // Last row of a non-final section draws the section separator; the footer
      // takes that role over from the last item when it renders.
      const isSectionBoundary =
        isLastInSection && !hasSectionFooter && !isLastSection;
      rows.push({
        id: `si:${sectionKey}:${itemKey}`,
        type: 'item',
        section,
        sectionIndex,
        item,
        itemIndex,
        separator: isSectionBoundary
          ? 'section'
          : isLastInSection
            ? null
            : 'item',
      });
    });

    if (hasSectionFooter) {
      rows.push({
        id: `sf:${sectionKey}`,
        type: 'sectionFooter',
        section,
        sectionIndex,
        separator: isLastSection ? null : 'section',
      });
    }
  });

  return { rows, stickyHeaderIndices };
};
