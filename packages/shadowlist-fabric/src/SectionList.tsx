import type { Ref, ReactElement } from 'react';
import { useMemo, useCallback, forwardRef } from 'react';
import ShadowList from './ShadowList';
import type {
  ShadowListCommands,
  SectionListProps,
  SectionListData,
} from './types';

/*
 * ShadowList renders single list, so this flattens `sections` into a single stream of
 * tagged rows: a header, the elements, then a footer for each section. The flat positions
 * of the header rows are collected into `stickyHeaderIndices` so native can pin them.
 */

type FlatRowType = 'sectionHeader' | 'element' | 'sectionFooter';

interface FlatRow<ElementT, SectionT> {
  id: string;
  type: FlatRowType;
  section: SectionListData<ElementT, SectionT>;
  sectionIndex: number;
  element?: ElementT;
  elementIndex?: number;
  // The caller's key for an element row (keyExtractor), matched against nonAnchorKeys.
  elementKey?: string;
  // Last element in its section (drives separators).
  isLastInSection?: boolean;
  // Last row of a non-final section (section separator).
  isSectionBoundary?: boolean;
}

/*
 * A separator slot may be a plain element or a function returning one; normalise to
 * an element or null.
 */
function renderComponent(
  component: ReactElement | (() => ReactElement | null) | null | undefined
): ReactElement | null {
  if (!component) return null;
  return typeof component === 'function' ? component() : component;
}

function SectionListInner<ElementT, SectionT = object>(
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
    overscan,
    nativeViewOverscan,
    getElementSizeSpec,
    measureLookaheadRows,
    nonAnchorKeys,
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
    accessible,
    accessibilityLabel,
    accessibilityRole,
    accessibilityHint,
    testID,
  }: SectionListProps<ElementT, SectionT>,
  ref: Ref<ShadowListCommands>
) {
  /*
   * Walk every section into the flat row stream, recording where each section-header
   * row lands so native knows which rows to pin.
   */
  const { data, stickyHeaderIndices } = useMemo(() => {
    const rows: FlatRow<ElementT, SectionT>[] = [];
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

      const lastElementIndex = section.data.length - 1;
      section.data.forEach((element, elementIndex) => {
        const elementKey = sectionKeyExtractor
          ? sectionKeyExtractor(element, elementIndex)
          : ((element as { id?: string })?.id ?? `${elementIndex}`);
        const isLastInSection = elementIndex === lastElementIndex;
        rows.push({
          id: `si:${sectionKey}:${elementKey}`,
          type: 'element',
          section,
          sectionIndex,
          element,
          elementIndex,
          elementKey,
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

  // The sticky overlay shows the header of whichever section is pinned at the top.
  const renderStickyHeaderOverlay = useCallback(
    (activeIndex: number) => {
      const row = data[activeIndex];
      if (!row || !renderSectionHeader) return null;
      return renderSectionHeader({ section: row.section });
    },
    [data, renderSectionHeader]
  );

  /*
   * The caller describes elements, not flattened rows: unwrap each element row and leave
   * headers and footers undescribed. Undefined when no getElementSizeSpec was supplied, so
   * the feature stays off.
   */
  const getRowSizeSpec = useMemo(
    () =>
      getElementSizeSpec
        ? (row: FlatRow<ElementT, SectionT>) =>
            row.type === 'element'
              ? getElementSizeSpec(
                  row.element as ElementT,
                  row.elementIndex as number,
                  row.section
                )
              : null
        : undefined,
    [getElementSizeSpec]
  );

  // nonAnchorKeys name elements by the caller's key; ShadowList sees the flattened row ids.
  const rowNonAnchorKeys = useMemo(() => {
    if (!nonAnchorKeys || nonAnchorKeys.length === 0) return undefined;
    const keys = new Set(nonAnchorKeys);
    return data
      .filter((row) => row.elementKey !== undefined && keys.has(row.elementKey))
      .map((row) => row.id);
  }, [data, nonAnchorKeys]);

  const elementSeparator = useMemo(
    () => renderComponent(ItemSeparatorComponent),
    [ItemSeparatorComponent]
  );
  const sectionSeparator = useMemo(
    () => renderComponent(SectionSeparatorComponent),
    [SectionSeparatorComponent]
  );

  /*
   * Render one flattened row based on its type: section header, section footer, or a
   * section element.
   */
  const renderRow = useCallback(
    ({
      element: row,
    }: {
      element: FlatRow<ElementT, SectionT>;
      index: number;
    }) => {
      if (row.type === 'sectionHeader') {
        return renderSectionHeader?.({ section: row.section }) ?? <></>;
      }

      if (row.type === 'sectionFooter') {
        return (
          <>
            {renderSectionFooter?.({ section: row.section })}
            {row.isSectionBoundary ? sectionSeparator : null}
          </>
        );
      }

      const sectionRenderElement = row.section.renderElement ?? renderElement;
      const content =
        sectionRenderElement?.({
          element: row.element as ElementT,
          index: row.elementIndex as number,
          section: row.section,
        }) ?? null;

      // Element separator between elements; section separator at a section boundary.
      let separator: ReactElement | null = null;
      if (row.isSectionBoundary) {
        separator = sectionSeparator;
      } else if (!row.isLastInSection) {
        separator = elementSeparator;
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
      elementSeparator,
      sectionSeparator,
    ]
  );

  return (
    <ShadowList
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
      overscan={overscan}
      nativeViewOverscan={nativeViewOverscan}
      getElementSizeSpec={getRowSizeSpec}
      measureLookaheadRows={measureLookaheadRows}
      nonAnchorKeys={rowNonAnchorKeys}
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
      accessible={accessible}
      accessibilityLabel={accessibilityLabel}
      accessibilityRole={accessibilityRole}
      accessibilityHint={accessibilityHint}
      testID={testID}
    />
  );
}

const SectionList = forwardRef(SectionListInner) as <
  ElementT,
  SectionT = object,
>(
  props: SectionListProps<ElementT, SectionT> & {
    ref?: Ref<ShadowListCommands>;
  }
) => ReactElement;

export default SectionList;
