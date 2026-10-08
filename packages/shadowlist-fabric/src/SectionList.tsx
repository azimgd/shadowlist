import type { ComponentType, Ref, ReactElement } from 'react';
import {
  useMemo,
  useCallback,
  useImperativeHandle,
  useRef,
  forwardRef,
} from 'react';
import { StyleSheet, View, type LayoutChangeEvent } from 'react-native';
import ShadowList from './ShadowList';
import {
  forwardedCommands,
  slTrace,
  slTraceEnabled,
  useStableElement,
} from './virtualizer';
import {
  flattenSections,
  type FlatRow,
  type SectionRows,
} from './virtualizer/sectionRows';
import {
  separatorComponentOf,
  sharedSeparatorOf,
} from './virtualizer/separators';
import { SectionIndex } from './virtualizer/SectionIndex';
import type {
  ItemSeparatorProps,
  RenderElementInfo,
  SectionItemSeparatorProps,
  SectionListCommands,
  SectionListLocation,
  SectionListProps,
  SectionSeparatorProps,
  ShadowListCommands,
} from './types';

/*
 * ShadowList renders one flat list. Each section becomes a header row, its elements,
 * then a footer row. The header positions go into stickyIndices so native can pin them.
 */

function renderComponent(
  component: ReactElement | (() => ReactElement | null) | null | undefined
): ReactElement | null {
  if (!component) return null;
  return typeof component === 'function' ? component() : component;
}

function sameIndices(
  previous: number[] | undefined,
  next: number[]
): previous is number[] {
  return (
    previous !== undefined &&
    previous.length === next.length &&
    previous.every((value, index) => value === next[index])
  );
}

function toRowIds<ElementT, SectionT>(
  rows: ReadonlyArray<FlatRow<ElementT, SectionT>>,
  elementKeys: ReadonlyArray<string> | undefined,
  previous: string[] | undefined
): string[] | undefined {
  if (!elementKeys || elementKeys.length === 0) return undefined;
  const keys = new Set(elementKeys);
  const ids: string[] = [];
  for (const row of rows) {
    if (row.elementKey !== undefined && keys.has(row.elementKey)) {
      ids.push(row.id);
    }
  }
  // The same ids keep the old array. Native gets no new prop.
  if (
    previous !== undefined &&
    previous.length === ids.length &&
    previous.every((id, index) => id === ids[index])
  ) {
    return previous;
  }
  return ids;
}

function SectionListInner<ElementT, SectionT = object>(
  {
    sections,
    renderElement,
    renderSectionHeader,
    renderSectionFooter,
    keyExtractor,
    stickySectionHeadersEnabled = true,
    sectionIndexTitles,
    sectionForSectionIndexTitle,
    ItemSeparatorComponent,
    SectionSeparatorComponent,
    getElementSizeSpec,
    nonAnchorKeys,
    persistentKeys,
    style,
    ...rest
  }: SectionListProps<ElementT, SectionT>,
  ref: Ref<SectionListCommands>
) {
  const innerRef = useRef<ShadowListCommands>(null);
  // The current sections, read by the renderers below so element rows don't carry the section.
  const sectionsRef = useRef(sections);
  sectionsRef.current = sections;

  /*
   * Rows from the last flatten, per section key. An unchanged row keeps its old object.
   * The list mounts rows by identity. Without this any change to sections re-renders
   * every row. An unchanged section is reused whole.
   */
  const previousSectionsRef = useRef<
    Map<string, SectionRows<ElementT, SectionT>>
  >(new Map());
  const previousIndicesRef = useRef<number[] | undefined>(undefined);

  const { data, stickyIndices } = useMemo(() => {
    const {
      rows,
      stickyIndices: flatIndices,
      nextSections,
    } = flattenSections(
      sections,
      keyExtractor,
      !!renderSectionHeader,
      !!renderSectionFooter,
      stickySectionHeadersEnabled,
      previousSectionsRef.current
    );
    previousSectionsRef.current = nextSections;

    /*
     * Header positions rarely move. A new array would force native to copy all props,
     * row keys included.
     */
    const indices = sameIndices(previousIndicesRef.current, flatIndices)
      ? previousIndicesRef.current
      : flatIndices;
    previousIndicesRef.current = indices;

    return { data: rows, stickyIndices: indices };
  }, [
    sections,
    keyExtractor,
    renderSectionHeader,
    renderSectionFooter,
    stickySectionHeadersEnabled,
  ]);

  /*
   * The pinned header's length along the scroll axis, for scrollToLocation to keep a row clear
   * of it. Measured on the pinned overlay, which is one view, not on every header row.
   */
  const pinnedLengthRef = useRef(0);
  const horizontal = rest.horizontal ?? false;
  const measuresHeaders = stickySectionHeadersEnabled && !!renderSectionHeader;
  const handlePinnedLayout = useCallback(
    (event: LayoutChangeEvent) => {
      const { width, height } = event.nativeEvent.layout;
      pinnedLengthRef.current = horizontal ? width : height;
    },
    [horizontal]
  );

  const renderStickyHeaderOverlay = useCallback(
    (activeIndex: number) => {
      const row = data[activeIndex];
      if (!row || !renderSectionHeader) return null;
      const section = row.section ?? sectionsRef.current[row.sectionIndex];
      if (!section) return null;
      return (
        <View onLayout={handlePinnedLayout}>
          {renderSectionHeader({ section })}
        </View>
      );
    },
    [data, renderSectionHeader, handlePinnedLayout]
  );

  const getRowSizeSpec = useMemo(
    () =>
      getElementSizeSpec
        ? (row: FlatRow<ElementT, SectionT>) => {
            const section = sectionsRef.current[row.sectionIndex];
            return row.type === 'element' && section
              ? getElementSizeSpec(
                  row.element as ElementT,
                  row.elementIndex as number,
                  section
                )
              : null;
          }
        : undefined,
    [getElementSizeSpec]
  );

  const previousNonAnchorRef = useRef<string[] | undefined>(undefined);
  const previousPersistentRef = useRef<string[] | undefined>(undefined);
  const rowNonAnchorKeys = useMemo(() => {
    const ids = toRowIds(data, nonAnchorKeys, previousNonAnchorRef.current);
    previousNonAnchorRef.current = ids;
    return ids;
  }, [data, nonAnchorKeys]);
  const rowPersistentKeys = useMemo(() => {
    const ids = toRowIds(data, persistentKeys, previousPersistentRef.current);
    previousPersistentRef.current = ids;
    return ids;
  }, [data, persistentKeys]);

  /*
   * Both separators go inside every row. An inline element would rebuild every mounted
   * row on each caller render. Separator components render per row with FlatList's props.
   */
  const elementSeparator = useStableElement(
    useMemo(
      () => renderComponent(sharedSeparatorOf(ItemSeparatorComponent)),
      [ItemSeparatorComponent]
    )
  );
  const sectionSeparator = useStableElement(
    useMemo(
      () => renderComponent(sharedSeparatorOf(SectionSeparatorComponent)),
      [SectionSeparatorComponent]
    )
  );
  const ElementSeparatorType = separatorComponentOf<
    SectionItemSeparatorProps<ElementT, SectionT>
  >(ItemSeparatorComponent);
  const SectionSeparatorType = separatorComponentOf<
    SectionSeparatorProps<ElementT, SectionT>
  >(SectionSeparatorComponent);

  /*
   * One separator component for the flat list. It picks the separator from the row above it:
   * a section separator at a section boundary, an item separator between two items of one
   * section, and nothing after a header.
   */
  const RowSeparator = useMemo(() => {
    // Shared separator elements stay inline in the rows, which costs no component per row.
    if (ElementSeparatorType === null && SectionSeparatorType === null) {
      return null;
    }
    return function SectionRowSeparator({
      highlighted,
      leadingItem: row,
      trailingItem: nextRow,
      ...separatorProps
    }: ItemSeparatorProps<FlatRow<ElementT, SectionT>>) {
      const allSections = sectionsRef.current;
      const section = row.section ?? allSections[row.sectionIndex];
      if (!section || row.type === 'sectionHeader') return null;
      if (row.isSectionBoundary) {
        if (SectionSeparatorType === null) return sectionSeparator;
        const nextSection = allSections[row.sectionIndex + 1];
        return (
          <SectionSeparatorType
            {...separatorProps}
            highlighted={highlighted}
            leadingItem={section.data[section.data.length - 1]}
            leadingSection={section}
            section={section}
            trailingItem={nextSection?.data[0]}
            trailingSection={nextSection}
          />
        );
      }
      if (row.type !== 'element' || row.isLastInSection) return null;
      if (ElementSeparatorType === null) return elementSeparator;
      return (
        <ElementSeparatorType
          {...separatorProps}
          highlighted={highlighted}
          leadingItem={row.element as ElementT}
          trailingItem={nextRow?.element as ElementT | undefined}
          section={section}
        />
      );
    } as ComponentType<ItemSeparatorProps<FlatRow<ElementT, SectionT>>>;
  }, [
    elementSeparator,
    sectionSeparator,
    ElementSeparatorType,
    SectionSeparatorType,
  ]);

  const renderRow = useCallback(
    (info: RenderElementInfo<FlatRow<ElementT, SectionT>>) => {
      const row = info.element;
      const section = row.section ?? sectionsRef.current[row.sectionIndex];

      if (row.type === 'sectionHeader') {
        return (section && renderSectionHeader?.({ section })) ?? <></>;
      }

      /*
       * Shared separator elements go inline, like before separator components. With a
       * component RowSeparator renders them instead.
       */
      const inlineSeparators = RowSeparator === null;

      if (row.type === 'sectionFooter') {
        return (
          <>
            {section ? renderSectionFooter?.({ section }) : null}
            {inlineSeparators && row.isSectionBoundary
              ? sectionSeparator
              : null}
          </>
        );
      }

      const sectionRenderElement = section?.renderElement ?? renderElement;
      const content =
        section && sectionRenderElement
          ? (sectionRenderElement({
              element: row.element as ElementT,
              index: row.elementIndex as number,
              section,
              separators: info.separators,
            }) ?? null)
          : null;

      let separator: ReactElement | null = null;
      if (inlineSeparators && row.isSectionBoundary) {
        separator = sectionSeparator;
      } else if (inlineSeparators && !row.isLastInSection) {
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
      RowSeparator,
      elementSeparator,
      sectionSeparator,
    ]
  );

  /*
   * scrollToLocation counts rows like React Native's SectionList: itemIndex 0 is the
   * section's header, or its first item when there are no headers, and 1 its first item.
   * With pinned headers a row lands below the section's header, like in React Native. The
   * header's length goes into viewOffset, scaled down toward viewPosition 1 where the header
   * no longer covers the row.
   */
  const dataRef = useRef(data);
  dataRef.current = data;
  const hasHeaders = !!renderSectionHeader;
  const scrollToLocation = useCallback(
    (params: SectionListLocation) => {
      const rows = dataRef.current;
      const first = rows.findIndex(
        (row) => row.sectionIndex === params.sectionIndex
      );
      if (first < 0) {
        innerRef.current?.scrollToIndex({ ...params, index: -1 });
        return;
      }
      const offset = hasHeaders
        ? params.itemIndex
        : Math.max(0, params.itemIndex - 1);
      let index = first + offset;
      const sectionRow = rows[index];
      if (!sectionRow || sectionRow.sectionIndex !== params.sectionIndex) {
        index = first;
      }
      const viewPosition = params.viewPosition ?? 0;
      let viewOffset = params.viewOffset ?? 0;
      if (measuresHeaders && index !== first) {
        viewOffset += pinnedLengthRef.current * (1 - viewPosition);
      }
      innerRef.current?.scrollToIndex({
        index,
        animated: params.animated,
        viewOffset,
        viewPosition,
      });
    },
    [hasHeaders, measuresHeaders]
  );

  useImperativeHandle(
    ref,
    () => ({
      ...forwardedCommands(innerRef),
      scrollToLocation,
      scrollToSection: (sectionIndex: number, animated?: boolean) =>
        scrollToLocation({ sectionIndex, itemIndex: 0, animated }),
    }),
    [scrollToLocation]
  );

  const sectionIndexTitlesRef = useRef(sectionForSectionIndexTitle);
  sectionIndexTitlesRef.current = sectionForSectionIndexTitle;
  const handleSectionIndexSelect = useCallback(
    (titleIndex: number) => {
      const title = sectionIndexTitles?.[titleIndex] ?? '';
      const sectionIndex =
        sectionIndexTitlesRef.current?.(title, titleIndex) ?? titleIndex;
      scrollToLocation({ sectionIndex, itemIndex: 0, animated: false });
    },
    [sectionIndexTitles, scrollToLocation]
  );

  /*
   * Device trace only. Logs which input gave the row renderer a new identity. A renderer that
   * changes every commit rebuilds every mounted row. Check this first when a list
   * re-renders more rows than changed.
   */
  const traceDepsRef = useRef<ReadonlyArray<unknown>>([]);
  if (slTraceEnabled()) {
    const deps = [
      renderElement,
      renderSectionHeader,
      renderSectionFooter,
      elementSeparator,
      sectionSeparator,
      data,
    ];
    const names = [
      'renderElement',
      'renderSectionHeader',
      'renderSectionFooter',
      'itemSeparator',
      'sectionSeparator',
      'data',
    ];
    const changed = names.filter(
      (_name, index) => traceDepsRef.current[index] !== deps[index]
    );
    traceDepsRef.current = deps;
    if (changed.length > 0) {
      slTrace(`section-deps changed=${changed.join(',')}`);
    }
  }

  const list = (
    <ShadowList
      {...rest}
      style={sectionIndexTitles?.length ? styles.fill : style}
      ref={innerRef}
      data={data}
      renderElement={renderRow}
      ItemSeparatorComponent={RowSeparator}
      stickyIndices={stickyIndices}
      renderStickyHeaderOverlay={renderStickyHeaderOverlay}
      getElementSizeSpec={getRowSizeSpec}
      nonAnchorKeys={rowNonAnchorKeys}
      persistentKeys={rowPersistentKeys}
    />
  );

  // The index floats over the list's trailing edge. Vertical lists only.
  if (!sectionIndexTitles?.length || rest.horizontal) return list;
  return (
    <View style={[styles.fill, style]}>
      {list}
      <SectionIndex
        titles={sectionIndexTitles}
        onSelect={handleSectionIndexSelect}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  fill: {
    flex: 1,
  },
});

const SectionList = forwardRef(SectionListInner) as <
  ElementT,
  SectionT = object,
>(
  props: SectionListProps<ElementT, SectionT> & {
    ref?: Ref<SectionListCommands>;
  }
) => ReactElement;

export default SectionList;
