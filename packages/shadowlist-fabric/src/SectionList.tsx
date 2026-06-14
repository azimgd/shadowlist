import type { Ref, ReactElement } from 'react';
import { useMemo, useCallback, forwardRef } from 'react';
import Shadowlist from './Shadowlist';
import type {
  ShadowlistCommands,
  SectionListProps,
  SectionListData,
} from './types';

/*
 * Flattens `sections` into one tagged element stream (section header, items, section
 * footer) for Shadowlist. Section headers carry their flat indices to
 * `stickyHeaderIndices` for native pinning.
 */

type FlatRowType = 'sectionHeader' | 'item' | 'sectionFooter';

interface FlatRow<ItemT, SectionT> {
  // Stable key for the flattened row, derived from section key and item key.
  id: string;
  type: FlatRowType;
  section: SectionListData<ItemT, SectionT>;
  sectionIndex: number;
  // Item payload (item rows only) and its index within the section.
  item?: ItemT;
  itemIndex?: number;
  // Last item in its section (drives separators).
  isLastInSection?: boolean;
  // Last row of a non-final section (section separator).
  isSectionBoundary?: boolean;
}

const renderComponent = (
  component: ReactElement | (() => ReactElement | null) | null | undefined
): ReactElement | null => {
  if (!component) return null;
  return typeof component === 'function' ? component() : component;
};

function SectionListInner<ItemT, SectionT = object>(
  {
    sections,
    renderElement,
    renderSectionHeader,
    renderSectionFooter,
    keyExtractor,
    stickySectionHeadersEnabled = true,
    ItemSeparatorComponent,
    SectionSeparatorComponent,
    ListHeaderComponent,
    ListFooterComponent,
    ListEmptyComponent,
    style,
    elementStyle,
    inverted,
    initialElementsSize,
    containerOffsetIndex,
    keyboardAvoidingEnabled,
    keyboardAvoidingOffset,
    refreshing,
    onRefresh,
    refreshColor,
    onScroll,
    onStartReached,
    onEndReached,
    onStartReachedThreshold,
    onEndReachedThreshold,
  }: SectionListProps<ItemT, SectionT>,
  ref: Ref<ShadowlistCommands>
) {
  // Build the tagged row stream plus the flat indices of section-header rows.
  const { data, stickyHeaderIndices } = useMemo(() => {
    const rows: FlatRow<ItemT, SectionT>[] = [];
    const stickyIndices: number[] = [];

    sections.forEach((section, sectionIndex) => {
      const sectionKey = section.key ?? `section-${sectionIndex}`;
      const sectionKeyExtractor = section.keyExtractor ?? keyExtractor;

      if (renderSectionHeader) {
        if (stickySectionHeadersEnabled) {
          stickyIndices.push(rows.length);
        }
        rows.push({
          id: `sh:${sectionKey}`,
          type: 'sectionHeader',
          section,
          sectionIndex,
        });
      }

      const lastItemIndex = section.data.length - 1;
      section.data.forEach((item, itemIndex) => {
        const itemKey = sectionKeyExtractor
          ? sectionKeyExtractor(item, itemIndex)
          : ((item as { id?: string })?.id ?? `${itemIndex}`);
        const isLastInSection = itemIndex === lastItemIndex;
        rows.push({
          id: `si:${sectionKey}:${itemKey}`,
          type: 'item',
          section,
          sectionIndex,
          item,
          itemIndex,
          isLastInSection,
          isSectionBoundary:
            isLastInSection &&
            !renderSectionFooter &&
            sectionIndex < sections.length - 1,
        });
      });

      if (renderSectionFooter) {
        rows.push({
          id: `sf:${sectionKey}`,
          type: 'sectionFooter',
          section,
          sectionIndex,
          isSectionBoundary: sectionIndex < sections.length - 1,
        });
      }
    });

    return { data: rows, stickyHeaderIndices: stickyIndices };
  }, [
    sections,
    keyExtractor,
    renderSectionHeader,
    renderSectionFooter,
    stickySectionHeadersEnabled,
  ]);

  // Render the active section's header for the sticky-header overlay.
  const renderStickyHeaderOverlay = useCallback(
    (activeIndex: number) => {
      const row = data[activeIndex];
      if (!row || !renderSectionHeader) return null;
      return renderSectionHeader({ section: row.section });
    },
    [data, renderSectionHeader]
  );

  const itemSeparator = useMemo(
    () => renderComponent(ItemSeparatorComponent),
    [ItemSeparatorComponent]
  );
  const sectionSeparator = useMemo(
    () => renderComponent(SectionSeparatorComponent),
    [SectionSeparatorComponent]
  );

  // Dispatch a flattened row to the right renderer.
  const renderRow = useCallback(
    ({ element }: { element: FlatRow<ItemT, SectionT>; index: number }) => {
      if (element.type === 'sectionHeader') {
        return renderSectionHeader?.({ section: element.section }) ?? <></>;
      }

      if (element.type === 'sectionFooter') {
        return (
          <>
            {renderSectionFooter?.({ section: element.section })}
            {element.isSectionBoundary ? sectionSeparator : null}
          </>
        );
      }

      const sectionRenderElement =
        element.section.renderElement ?? renderElement;
      const content =
        sectionRenderElement?.({
          element: element.item as ItemT,
          index: element.itemIndex as number,
          section: element.section,
        }) ?? null;

      // Item separator between items; section separator at a section boundary.
      let separator: ReactElement | null = null;
      if (element.isSectionBoundary) {
        separator = sectionSeparator;
      } else if (!element.isLastInSection) {
        separator = itemSeparator;
      }

      return (
        <>
          {content}
          {separator}
        </>
      );
    },
    [
      renderElement,
      renderSectionHeader,
      renderSectionFooter,
      itemSeparator,
      sectionSeparator,
    ]
  );

  return (
    <Shadowlist
      ref={ref}
      data={data}
      renderElement={renderRow}
      stickyHeaderIndices={stickyHeaderIndices}
      renderStickyHeaderOverlay={renderStickyHeaderOverlay}
      style={style}
      elementStyle={elementStyle}
      inverted={inverted}
      initialElementsSize={initialElementsSize}
      containerOffsetIndex={containerOffsetIndex}
      keyboardAvoidingEnabled={keyboardAvoidingEnabled}
      keyboardAvoidingOffset={keyboardAvoidingOffset}
      refreshing={refreshing}
      onRefresh={onRefresh}
      refreshColor={refreshColor}
      onScroll={onScroll}
      onStartReached={onStartReached}
      onEndReached={onEndReached}
      onStartReachedThreshold={onStartReachedThreshold}
      onEndReachedThreshold={onEndReachedThreshold}
      ListHeaderComponent={ListHeaderComponent}
      ListFooterComponent={ListFooterComponent}
      ListEmptyComponent={ListEmptyComponent}
    />
  );
}

// Cast preserves the generic item/section types for callers.
const SectionList = forwardRef(SectionListInner) as <ItemT, SectionT = object>(
  props: SectionListProps<ItemT, SectionT> & { ref?: Ref<ShadowlistCommands> }
) => ReactElement;

export default SectionList;
