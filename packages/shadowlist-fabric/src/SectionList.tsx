import type { Ref, ReactElement } from 'react';
import { useMemo, useCallback, forwardRef } from 'react';
import Shadowlist from './Shadowlist';
import type {
  ShadowlistCommands,
  SectionListProps,
  SectionListData,
} from './types';

/*
 * SectionList is a thin data layer over Shadowlist: it flattens `sections` into one
 * tagged element stream (section header, items, section footer) and hands that to
 * the virtualizer, exactly how React Native's SectionList flattens onto
 * VirtualizedList. Section headers carry their flat indices to `stickyHeaderIndices`
 * so the native layer pins and swaps them on the UI thread. Everything else -
 * virtualization, measurement, maintain-visible-content-position, scrollToIndex -
 * is the unchanged Shadowlist engine, so sections cost nothing extra.
 */

type FlatRowType = 'sectionHeader' | 'item' | 'sectionFooter';

interface FlatRow<ItemT, SectionT> {
  /*
   * Stable key for the flattened row (Shadowlist keys on element.id). Derived from
   * the section key and, for items, the item key so reorders reconcile cleanly.
   */
  id: string;
  type: FlatRowType;
  section: SectionListData<ItemT, SectionT>;
  sectionIndex: number;
  /*
   * Item payload (item rows only) and its index within the section.
   */
  item?: ItemT;
  itemIndex?: number;
  /*
   * Whether an item row is the last item in its section (drives separators).
   */
  isLastInSection?: boolean;
  /*
   * Whether this row is the last row of a non-final section (section separator).
   */
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
    renderItem,
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
  /*
   * Flatten the sections once per `sections` change into the tagged row stream and,
   * alongside it, the flat indices of the section-header rows (for native pinning).
   */
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

  /*
   * Content for the always-mounted sticky-header overlay: Shadowlist passes the flat
   * index of the active section's header row; render that section's header. Pinned
   * natively, so the overlay's position is never a frame behind the scroll.
   */
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

  /*
   * Dispatch a flattened row to the right renderer. Passed as Shadowlist's
   * renderElement prop (a render callback, not a mounted component) - Shadowlist
   * wraps each row in its own memoized element host.
   */
  const renderElement = useCallback(
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

      const sectionRenderItem = element.section.renderItem ?? renderItem;
      const content =
        sectionRenderItem?.({
          item: element.item as ItemT,
          index: element.itemIndex as number,
          section: element.section,
        }) ?? null;

      /*
       * Item separators sit between items in a section; a section boundary row
       * (last item of a non-final section with no footer) gets the section
       * separator instead.
       */
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
      renderItem,
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
      renderElement={renderElement}
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

/*
 * forwardRef + generics: the cast preserves the generic item/section types for
 * callers while still forwarding the Shadowlist imperative handle.
 */
const SectionList = forwardRef(SectionListInner) as <ItemT, SectionT = object>(
  props: SectionListProps<ItemT, SectionT> & { ref?: Ref<ShadowlistCommands> }
) => ReactElement;

export default SectionList;
