import type { Ref, ReactElement } from 'react';
import { useMemo, useCallback, forwardRef } from 'react';
import Shadowlist from './Shadowlist';
import { flattenSections, type SectionFlatRow } from './flattenSections';
import { renderComponent } from './renderComponent';
import type { ShadowlistCommands, SectionListProps } from './types';

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
  // Build the tagged row stream plus the flat indices of section-header rows
  // (see flattenSections).
  const { data, stickyHeaderIndices } = useMemo(() => {
    const { rows, stickyHeaderIndices: stickyIndices } = flattenSections<
      ItemT,
      SectionT
    >({
      sections,
      keyExtractor,
      hasSectionHeader: !!renderSectionHeader,
      hasSectionFooter: !!renderSectionFooter,
      stickySectionHeadersEnabled,
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

  // Dispatch a flattened row to the right renderer; the flattener already decided
  // which separator follows each row.
  const renderElement = useCallback(
    ({
      element,
    }: {
      element: SectionFlatRow<ItemT, SectionT>;
      index: number;
    }) => {
      const separator =
        element.separator === 'section'
          ? sectionSeparator
          : element.separator === 'item'
            ? itemSeparator
            : null;

      if (element.type === 'sectionHeader') {
        return renderSectionHeader?.({ section: element.section }) ?? <></>;
      }

      if (element.type === 'sectionFooter') {
        return (
          <>
            {renderSectionFooter?.({ section: element.section })}
            {separator}
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

// Cast preserves the generic item/section types for callers.
const SectionList = forwardRef(SectionListInner) as <ItemT, SectionT = object>(
  props: SectionListProps<ItemT, SectionT> & { ref?: Ref<ShadowlistCommands> }
) => ReactElement;

export default SectionList;
