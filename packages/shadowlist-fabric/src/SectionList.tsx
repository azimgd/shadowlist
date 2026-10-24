import {
  useMemo,
  useCallback,
  useImperativeHandle,
  useRef,
  forwardRef,
  type ComponentType,
  type Ref,
  type ReactElement,
} from 'react';
import { StyleSheet, View, type LayoutChangeEvent } from 'react-native';
import { ShadowListBase } from './ShadowList';
import {
  forwardedCommands,
  renderComponent,
  useInputTrace,
  useStableReactElement,
} from './virtualizer';
import { useListStore } from './virtualizer/nativeStore';
import {
  buildSectionSource,
  createSectionSourceState,
  type FlatRow,
  type SectionSourceState,
} from './virtualizer/sectionSource';
import {
  separatorComponentOf,
  sharedSeparatorOf,
} from './virtualizer/separators';
import { SectionIndex } from './virtualizer/SectionIndex';
import type {
  ItemSeparatorProps,
  RenderItemInfo,
  SectionItemSeparatorProps,
  SectionListCommands,
  SectionListLocation,
  SectionListProps,
  SectionSeparatorProps,
  ShadowListCommands,
} from './types';

/*
 * ShadowList renders one flat list. Each section becomes a header row, its items,
 * then a footer row. JS keys the items, and native builds the rows' keys and pins the
 * headers, see sectionSource.ts. Rows are only built for the rows that mount.
 */

/*
 * The SectionList inputs that give the row renderer a new identity, for the device trace.
 */
const SECTION_TRACE_NAMES = [
  'renderItem',
  'renderSectionHeader',
  'renderSectionFooter',
  'itemSeparator',
  'sectionSeparator',
  'source',
];

/*
 * The row keys of the items named by keys, keeping the previous array when they come out the
 * same so native gets no new prop.
 */
function sameKeys(
  previous: string[] | undefined,
  next: string[]
): previous is string[] {
  return (
    previous !== undefined &&
    previous.length === next.length &&
    previous.every((key, index) => key === next[index])
  );
}

function SectionListInner<ItemT, SectionT = object>(
  {
    sections,
    renderItem,
    renderSectionHeader,
    renderSectionFooter,
    keyExtractor,
    stickySectionHeadersEnabled = true,
    sectionIndexTitles,
    sectionForSectionIndexTitle,
    ItemSeparatorComponent,
    SectionSeparatorComponent,
    getItemSizeSpec,
    nonAnchorKeys,
    persistentKeys,
    style,
    ...rest
  }: SectionListProps<ItemT, SectionT>,
  ref: Ref<SectionListCommands>
) {
  const innerRef = useRef<ShadowListCommands>(null);
  // The current sections, read by the renderers below so item rows don't carry the section.
  const sectionsRef = useRef(sections);
  sectionsRef.current = sections;

  /*
   * What the last flatten wrote, per section key. A section that comes back with the same
   * items keeps its keys without calling keyExtractor, and native reuses its rows. Rows keep
   * their objects while nothing they render from changed, since the list mounts rows by
   * identity.
   */
  const store = useListStore();
  const sourceStateRef = useRef<SectionSourceState<ItemT, SectionT> | null>(
    null
  );
  if (sourceStateRef.current === null) {
    sourceStateRef.current = createSectionSourceState();
  }
  const sourceState = sourceStateRef.current;
  const hasHeader = !!renderSectionHeader;
  const hasFooter = !!renderSectionFooter;
  const source = useMemo(
    () =>
      buildSectionSource(
        sourceState,
        sections,
        keyExtractor,
        {
          hasHeader,
          hasFooter,
          stickyEnabled: stickySectionHeadersEnabled,
        },
        store
      ),
    [
      sourceState,
      sections,
      keyExtractor,
      hasHeader,
      hasFooter,
      stickySectionHeadersEnabled,
      store,
    ]
  );

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
      const row = source.itemAt(activeIndex);
      if (!row || !renderSectionHeader) return null;
      const section = row.section ?? sectionsRef.current[row.sectionIndex];
      if (!section) return null;
      return (
        <View onLayout={handlePinnedLayout}>
          {renderSectionHeader({ section })}
        </View>
      );
    },
    [source, renderSectionHeader, handlePinnedLayout]
  );

  const getRowSizeSpec = useMemo(
    () =>
      getItemSizeSpec
        ? (row: FlatRow<ItemT, SectionT>) => {
            const section = sectionsRef.current[row.sectionIndex];
            return row.type === 'item' && section
              ? getItemSizeSpec(
                  row.item as ItemT,
                  row.itemIndex as number,
                  section
                )
              : null;
          }
        : undefined,
    [getItemSizeSpec]
  );

  const previousNonAnchorRef = useRef<string[] | undefined>(undefined);
  const previousPersistentRef = useRef<string[] | undefined>(undefined);
  const rowNonAnchorKeys = useMemo(() => {
    if (!nonAnchorKeys || nonAnchorKeys.length === 0) return undefined;
    const keys = source.rowKeysOfItems(nonAnchorKeys);
    const previous = previousNonAnchorRef.current;
    previousNonAnchorRef.current = sameKeys(previous, keys) ? previous : keys;
    return previousNonAnchorRef.current;
  }, [source, nonAnchorKeys]);
  const rowPersistentKeys = useMemo(() => {
    if (!persistentKeys || persistentKeys.length === 0) return undefined;
    const keys = source.rowKeysOfItems(persistentKeys);
    const previous = previousPersistentRef.current;
    previousPersistentRef.current = sameKeys(previous, keys) ? previous : keys;
    return previousPersistentRef.current;
  }, [source, persistentKeys]);

  /*
   * Both separators go inside every row. An inline React element would rebuild every mounted
   * row on each caller render. Separator components render per row with their own props.
   */
  const itemSeparator = useStableReactElement(
    useMemo(
      () => renderComponent(sharedSeparatorOf(ItemSeparatorComponent)),
      [ItemSeparatorComponent]
    )
  );
  const sectionSeparator = useStableReactElement(
    useMemo(
      () => renderComponent(sharedSeparatorOf(SectionSeparatorComponent)),
      [SectionSeparatorComponent]
    )
  );
  const ItemSeparatorType = separatorComponentOf<
    SectionItemSeparatorProps<ItemT, SectionT>
  >(ItemSeparatorComponent);
  const SectionSeparatorType = separatorComponentOf<
    SectionSeparatorProps<ItemT, SectionT>
  >(SectionSeparatorComponent);

  /*
   * One separator component for the flat list. It picks the separator from the row above it:
   * a section separator at a section boundary, an item separator between two items of one
   * section, and nothing after a header.
   */
  const RowSeparator = useMemo(() => {
    // Shared separator React elements stay inline in the rows, which costs no component per row.
    if (ItemSeparatorType === null && SectionSeparatorType === null) {
      return null;
    }
    return function SectionRowSeparator({
      highlighted,
      leadingItem: row,
      trailingItem: nextRow,
      ...separatorProps
    }: ItemSeparatorProps<FlatRow<ItemT, SectionT>>) {
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
      if (row.type !== 'item' || row.isLastInSection) return null;
      if (ItemSeparatorType === null) return itemSeparator;
      return (
        <ItemSeparatorType
          {...separatorProps}
          highlighted={highlighted}
          leadingItem={row.item as ItemT}
          trailingItem={nextRow?.item as ItemT | undefined}
          section={section}
        />
      );
    } as ComponentType<ItemSeparatorProps<FlatRow<ItemT, SectionT>>>;
  }, [
    itemSeparator,
    sectionSeparator,
    ItemSeparatorType,
    SectionSeparatorType,
  ]);

  const renderRow = useCallback(
    (info: RenderItemInfo<FlatRow<ItemT, SectionT>>) => {
      const row = info.item;
      const section = row.section ?? sectionsRef.current[row.sectionIndex];

      if (row.type === 'sectionHeader') {
        return (section && renderSectionHeader?.({ section })) ?? <></>;
      }

      /*
       * Shared separator React elements go inline. With a separator component, RowSeparator
       * renders them instead.
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

      const sectionRenderItem = section?.renderItem ?? renderItem;
      const content =
        section && sectionRenderItem
          ? (sectionRenderItem({
              item: row.item as ItemT,
              index: row.itemIndex as number,
              section,
              separators: info.separators,
            }) ?? null)
          : null;

      let separator: ReactElement | null = null;
      if (inlineSeparators && row.isSectionBoundary) {
        separator = sectionSeparator;
      } else if (inlineSeparators && !row.isLastInSection) {
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
      RowSeparator,
      itemSeparator,
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
  const sourceRef = useRef(source);
  sourceRef.current = source;
  const hasHeaders = hasHeader;
  const scrollToLocation = useCallback(
    (params: SectionListLocation) => {
      const rows = sourceRef.current;
      const first = rows.firstRowOfSection(params.sectionIndex);
      if (first < 0) {
        innerRef.current?.scrollToIndex({ ...params, index: -1 });
        return;
      }
      const offset = hasHeaders
        ? params.itemIndex
        : Math.max(0, params.itemIndex - 1);
      let index = first + offset;
      const sectionRow = rows.itemAt(index);
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

  useInputTrace('section-deps', SECTION_TRACE_NAMES, [
    renderItem,
    renderSectionHeader,
    renderSectionFooter,
    itemSeparator,
    sectionSeparator,
    source,
  ]);

  const list = (
    <ShadowListBase
      {...rest}
      style={sectionIndexTitles?.length ? styles.fill : style}
      ref={innerRef}
      source={source}
      store={store}
      renderItem={renderRow}
      ItemSeparatorComponent={RowSeparator}
      renderStickyHeaderOverlay={renderStickyHeaderOverlay}
      getItemSizeSpec={getRowSizeSpec}
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

const SectionList = forwardRef(SectionListInner) as <ItemT, SectionT = object>(
  props: SectionListProps<ItemT, SectionT> & {
    ref?: Ref<SectionListCommands>;
  }
) => ReactElement;

export default SectionList;
