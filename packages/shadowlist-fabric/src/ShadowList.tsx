import {
  useRef,
  useMemo,
  useCallback,
  useState,
  forwardRef,
  type ComponentRef,
  type Ref,
  type ReactElement,
} from 'react';
import {
  Platform,
  StyleSheet,
  View,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import ShadowListView, {
  type OnAtEndChange,
  type OnContentSizeChange,
} from './ShadowListViewNativeComponent';
import ShadowListTemplateView from './ShadowListTemplateViewNativeComponent';
import type {
  ShadowListProps,
  ShadowListCommands,
  ViewabilityConfigCallbackPair,
} from './types';
import {
  CellRenderer,
  SNAP_ALIGNMENT,
  SHADOWLIST_OVERSCAN,
  SHADOWLIST_OVERSCAN_LEADING,
  useMountedRange,
  useRefreshDefer,
  useDragReorder,
  useDragBoardChecks,
  usePersistentKeys,
  useViewability,
  useImperativeCommands,
  useRowSizeSpecs,
  useStableReactElement,
  useRowSelection,
  usePrefetch,
  useAnchorState,
  useKeyboardDismissResponder,
  useKeySelection,
  useRenderTraceStart,
  useRenderTrace,
  slTrace,
  slTraceEnabled,
  nativeTagOf,
  defaultKeyExtractor,
  renderComponent,
  createRowIndexStore,
  type RowIndexStore,
  type CommandSource,
} from './virtualizer';
import { useListStore, type ListStore } from './virtualizer/nativeStore';
import { usePlainSource, type RowSource } from './virtualizer/rowSource';
import {
  SeparatorStore,
  separatorComponentOf,
  sharedSeparatorOf,
} from './virtualizer/separators';
import {
  contentPadding,
  crossPadding,
  footerPaddingStyles,
  headerPaddingStyles,
  rowPaddingStyles,
} from './virtualizer/contentPadding';

export { initialMountedRange, type MountedRange } from './virtualizer';

/*
 * Shared empty arrays. An inline [] is a new object on every render, which forces a deep
 * props diff.
 */
const EMPTY_STRINGS: ReadonlyArray<string> = [];
const EMPTY_NUMBERS: ReadonlyArray<number> = [];

/*
 * Default extra rows on each side of the mounted rows that prefetchDataSource hears about.
 */
const PREFETCH_ROWS = 10;

/*
 * The named deceleration rates.
 */
const DECELERATION_RATES = {
  normal: Platform.OS === 'ios' ? 0.998 : 0.985,
  fast: Platform.OS === 'ios' ? 0.99 : 0.9,
};

/*
 * ShadowList's props with its rows given as a source instead of data. SectionList and TreeList
 * render this with their own sources and never build an array of every row.
 */
export interface ShadowListBaseProps<ItemT> extends Omit<
  ShadowListProps<ItemT>,
  'data' | 'keyExtractor'
> {
  source: RowSource<ItemT>;
  store: ListStore | null;
}

/*
 * The JS side of the native ShadowListView. It mounts only the rows near the screen and
 * passes native scroll, drag and refresh events to the hooks below.
 */
function ShadowListBaseInner<ItemT>(
  {
    source: sourceProp,
    store,
    renderItem,
    style,
    itemStyle,
    inverted = false,
    followAppends = false,
    onAtEndChange,
    horizontal = false,
    stickyHeader = false,
    stickyFooter = false,
    autoHideHeader = false,
    autoHideFooter = false,
    reorderEnabled = false,
    onMoveItem,
    dragBoardId,
    dragListId,
    onMoveItemToList,
    numberOfColumns = 1,
    overscan = 1,
    mountOverscanRows = SHADOWLIST_OVERSCAN,
    mountOverscanRowsLeading = SHADOWLIST_OVERSCAN_LEADING,
    getItemSizeSpec,
    measureLookaheadRows = 48,
    persistentKeys,
    nonAnchorKeys,
    stickyIndices,
    renderStickyHeaderOverlay,
    initialScrollIndex,
    scrollIndex: scrollIndexProp,
    trackItemSizes = false,
    extraData,
    refreshing = false,
    onRefresh,
    refreshColor,
    progressViewOffset = 0,
    initialNumToRender = 20,
    onStartReached,
    onEndReached,
    onStartReachedThreshold = 1,
    onEndReachedThreshold = 1,
    onScroll,
    onScrollBeginDrag,
    onScrollEndDrag,
    onMomentumScrollBegin,
    onMomentumScrollEnd,
    onContentSizeChange,
    scrollEventThrottle = 0,
    onScrollToIndexFailed,
    scrollEnabled = true,
    showsVerticalScrollIndicator = true,
    showsHorizontalScrollIndicator = true,
    bounces = true,
    decelerationRate,
    scrollsToTop = true,
    keyboardDismissMode = 'none',
    keyboardShouldPersistTaps,
    nestedScrollEnabled = false,
    contentContainerStyle,
    contentInset,
    snapToItem = false,
    snapAlignment = 'start',
    viewabilityConfig,
    onViewableItemsChanged,
    viewabilityConfigCallbackPairs,
    allowsMultipleSelection = false,
    selectedKeys,
    onSelectionChange,
    shouldSelectItem,
    enableSelectionOnKeyPress = false,
    onActivateItem,
    leadingSwipeActionsForItem,
    trailingSwipeActionsForItem,
    contextMenuForItem,
    prefetchDataSource,
    prefetchRows = PREFETCH_ROWS,
    ItemSeparatorComponent,
    ListHeaderComponent,
    ListHeaderComponentStyle,
    ListFooterComponent,
    ListFooterComponentStyle,
    ListEmptyComponent,
    columnWrapperStyle,
    accessible,
    accessibilityLabel,
    accessibilityRole,
    accessibilityHint,
    testID,
  }: ShadowListBaseProps<ItemT>,
  ref: Ref<ShadowListCommands>
) {
  const shadowlistViewRef = useRef<ComponentRef<typeof ShadowListView> | null>(
    null
  );

  const traceRenderStartRef = useRenderTraceStart();

  /*
   * initialScrollIndex only counts on mount. scrollIndex scrolls
   * whenever it changes and wins when both are set.
   */
  const [initialIndex] = useState(initialScrollIndex);
  const scrollIndex =
    scrollIndexProp ??
    (initialIndex != null && initialIndex >= 0 ? initialIndex : -2);

  const handleRefresh = useCallback(() => {
    if (slTraceEnabled()) {
      slTrace(`refresh pull id=${nativeTagOf(shadowlistViewRef.current)}`);
    }
    onRefresh?.();
  }, [onRefresh]);

  /*
   * Wrap the edge handlers once. The trace then sees every native call and the
   * handler doesn't change whenever the caller's does.
   */
  const edgeHandlersRef = useRef({
    onStartReached,
    onEndReached,
    onAtEndChange,
  });
  edgeHandlersRef.current = { onStartReached, onEndReached, onAtEndChange };
  const handleStartReached = useCallback(() => {
    if (slTraceEnabled()) {
      slTrace(`reached start id=${nativeTagOf(shadowlistViewRef.current)}`);
    }
    edgeHandlersRef.current.onStartReached?.();
  }, []);
  const handleEndReached = useCallback(() => {
    if (slTraceEnabled()) {
      slTrace(`reached end id=${nativeTagOf(shadowlistViewRef.current)}`);
    }
    edgeHandlersRef.current.onEndReached?.();
  }, []);
  const handleAtEndChange = useCallback(
    (event: { nativeEvent: OnAtEndChange }) => {
      if (slTraceEnabled()) {
        slTrace(
          `at end ${event.nativeEvent.atEnd ? 1 : 0} id=${nativeTagOf(shadowlistViewRef.current)}`
        );
      }
      edgeHandlersRef.current.onAtEndChange?.(event.nativeEvent.atEnd);
    },
    []
  );

  /*
   * During a refresh, hold new data until the spinner is gone so rows don't jump under
   * the user. source is what we actually render.
   */
  const { data: source, handleRefreshSettle } = useRefreshDefer({
    data: sourceProp,
    refreshing,
    onRefresh,
    inverted,
    horizontal,
  });
  const rowCount = source.count;

  /*
   * Shared with every row. A row that skipped a move still answers a late index read.
   * Written in render, before the rows render, like each row's own index.
   */
  const rowIndexRef = useRef<RowIndexStore | null>(null);
  if (rowIndexRef.current === null) rowIndexRef.current = createRowIndexStore();
  const rowIndex = rowIndexRef.current;
  rowIndex.source = source;

  /*
   * Never mount fewer rows ahead during a fling than at rest. A fling needs them most.
   */
  const mountOverscanRowsAhead = Math.max(
    mountOverscanRows,
    mountOverscanRowsLeading
  );
  const {
    mountedIndices,
    mountedRangeProps,
    handleMountedRangeChange,
    seedAroundIndex,
  } = useMountedRange({
    count: rowCount,
    indexOfKey: source.indexOfKey,
    initialNumToRender,
    inverted,
    followAppends,
    scrollIndex,
    mountOverscanRows,
    mountOverscanRowsLeading: mountOverscanRowsAhead,
  });

  const {
    renderIndices: draggedIndices,
    handleDragStart,
    handleDragEnd,
  } = useDragReorder({
    source,
    mountedIndices,
    reorderEnabled,
    onMoveItem,
    listId: dragListId,
    onMoveItemToList,
  });
  useDragBoardChecks(dragBoardId, dragListId);

  const renderIndices = usePersistentKeys({
    source,
    persistentKeys,
    renderIndices: draggedIndices,
  });

  /*
   * One viewability pair per config, the single config and callback included.
   */
  const viewabilityPairs = useMemo<
    ReadonlyArray<ViewabilityConfigCallbackPair<ItemT>>
  >(() => {
    if (viewabilityConfigCallbackPairs) return viewabilityConfigCallbackPairs;
    if (!onViewableItemsChanged) return [];
    return [
      { viewabilityConfig: viewabilityConfig ?? {}, onViewableItemsChanged },
    ];
  }, [
    viewabilityConfigCallbackPairs,
    viewabilityConfig,
    onViewableItemsChanged,
  ]);

  const {
    activeStickyIndex,
    viewableRules,
    viewableTimings,
    viewabilityInteracted,
    handleViewabilityChange,
    recordInteraction,
    visibleRangeRef,
  } = useViewability({
    source,
    pairs: viewabilityPairs,
  });

  /*
   * Row sizes by key when trackItemSizes is on, null when off. A ref, since only
   * imperative calls read it.
   */
  const itemSizesRef = useRef<Map<string, number> | null>(null);
  if (trackItemSizes) {
    if (itemSizesRef.current === null) itemSizesRef.current = new Map();
  } else if (itemSizesRef.current !== null) {
    /*
     * Tracking was turned off. Drop the map. The layout callbacks are gone too, and
     * nothing would keep it up to date.
     */
    itemSizesRef.current = null;
  }

  /*
   * A horizontal list styled height: 'auto' takes the height of its tallest mounted row. Its
   * rows keep their own height instead of filling the list's, and it drops the default flex.
   */
  const autoHeight =
    horizontal &&
    numberOfColumns === 1 &&
    StyleSheet.flatten(style)?.height === 'auto';
  const autoHeightRef = useRef(autoHeight);
  autoHeightRef.current = autoHeight;
  const rowHeightsRef = useRef(new Map<string, number>());
  const [tallestRowHeight, setTallestRowHeight] = useState(0);
  const measureTallestRow = useCallback(() => {
    let tallest = 0;
    for (const height of rowHeightsRef.current.values()) {
      tallest = Math.max(tallest, height);
    }
    setTallestRowHeight(tallest);
  }, []);

  const handleCellLayout = useCallback(
    (key: string, width: number, height: number) => {
      itemSizesRef.current?.set(key, horizontal ? width : height);
      if (autoHeightRef.current) {
        rowHeightsRef.current.set(key, height);
        measureTallestRow();
      }
    },
    [horizontal, measureTallestRow]
  );

  const handleCellRelease = useCallback(
    (key: string) => {
      itemSizesRef.current?.delete(key);
      if (rowHeightsRef.current.delete(key)) measureTallestRow();
    },
    [measureTallestRow]
  );
  const tracksCells = trackItemSizes || autoHeight;

  /*
   * shouldSelectItem by index and by key, read when a row asks to be selected.
   */
  const selectableRef = useRef({ source, shouldSelectItem });
  selectableRef.current = { source, shouldSelectItem };
  const canSelectIndex = useCallback((index: number) => {
    const { source: currentSource, shouldSelectItem: shouldSelect } =
      selectableRef.current;
    const item = currentSource.itemAt(index);
    return item !== undefined && (shouldSelect?.({ item, index }) ?? true);
  }, []);
  const canSelect = useCallback(
    (key: string) => {
      const index = selectableRef.current.source.indexOfKey(key);
      return index >= 0 && canSelectIndex(index);
    },
    [canSelectIndex]
  );

  const { selectedKeySet, selection, selectionStateRef, selectedIndices } =
    useRowSelection({
      source,
      store,
      selectedKeys,
      allowsMultipleSelection,
      onSelectionChange,
      canSelect,
    });

  const keySelectionEnabled =
    enableSelectionOnKeyPress && Platform.OS === 'macos';
  const keySelectionProps = useKeySelection({
    enabled: keySelectionEnabled,
    source,
    selectionStateRef,
    selection,
    canSelectIndex,
    onActivateItem,
    visibleRangeRef,
    seedAroundIndex,
    viewRef: shadowlistViewRef,
  });

  const { nativePrefetchRows, handlePrefetchChange } = usePrefetch({
    prefetchDataSource,
    prefetchRows,
  });
  const mountedHigh = mountedIndices[mountedIndices.length - 1] ?? -1;

  /*
   * The content length along the scroll axis, for onScrollToIndexFailed's average row length.
   * Native only sends it while onContentSizeChange is set. Otherwise the average is 0.
   */
  const contentLengthRef = useRef(0);
  const onContentSizeChangeRef = useRef(onContentSizeChange);
  onContentSizeChangeRef.current = onContentSizeChange;
  const handleContentSizeChange = useCallback(
    (event: { nativeEvent: OnContentSizeChange }) => {
      const { width, height } = event.nativeEvent;
      contentLengthRef.current = horizontal ? width : height;
      onContentSizeChangeRef.current?.(width, height);
    },
    [horizontal]
  );

  const { handleAnchorState, requestAnchorState, restoreAnchorState } =
    useAnchorState({
      viewRef: shadowlistViewRef,
      source,
      seedAroundIndex,
    });

  const commandSourceRef = useRef<CommandSource | null>(null);
  const commandSource: CommandSource = {
    count: rowCount,
    indexOfItem: source.indexOfItem,
    highestMountedIndex: mountedHigh,
    contentLength: contentLengthRef.current,
    onScrollToIndexFailed,
    seedAroundIndex,
    recordInteraction,
    selectIndex: (index: number) => {
      const key = source.keyAt(index);
      if (key !== undefined) selection.select(key);
    },
    deselectIndex: (index: number) => {
      const key = source.keyAt(index);
      if (key !== undefined) selection.deselect(key);
    },
    getSelectedIndices: selectedIndices,
    requestAnchorState,
    restoreAnchorState,
  };
  commandSourceRef.current = commandSource;

  useImperativeCommands(
    ref,
    shadowlistViewRef,
    itemSizesRef,
    commandSourceRef as { current: CommandSource }
  );

  /*
   * Padding from contentContainerStyle and contentInset. Along the scroll axis it goes into the
   * header and footer, across it into every row and template.
   */
  const padding = useMemo(
    () => contentPadding(contentContainerStyle, contentInset, horizontal),
    [contentContainerStyle, contentInset, horizontal]
  );
  const columnPaddings = useMemo(
    () =>
      rowPaddingStyles(
        numberOfColumns,
        padding.crossStart,
        padding.crossEnd,
        columnWrapperStyle,
        horizontal
      ),
    [
      numberOfColumns,
      padding.crossStart,
      padding.crossEnd,
      columnWrapperStyle,
      horizontal,
    ]
  );

  const cellDimensionStyle = useMemo<StyleProp<ViewStyle>>(() => {
    if (horizontal) {
      if (autoHeight) return null;
      return numberOfColumns > 1
        ? { height: `${100 / numberOfColumns}%` }
        : styles.cellHorizontal;
    } else {
      return numberOfColumns > 1
        ? { width: `${100 / numberOfColumns}%` }
        : styles.cellVertical;
    }
  }, [horizontal, numberOfColumns, autoHeight]);

  /*
   * One row style per column. Columns differ only when they carry padding.
   */
  const cellColumnStyles = useMemo<StyleProp<ViewStyle>[]>(() => {
    const base: StyleProp<ViewStyle>[] = itemStyle
      ? [styles.cell, cellDimensionStyle, itemStyle]
      : [styles.cell, cellDimensionStyle];
    if (!columnPaddings) return [base];
    return columnPaddings.map((paddingStyle) => [...base, paddingStyle]);
  }, [cellDimensionStyle, itemStyle, columnPaddings]);

  /*
   * Precomputed sizes for rows near the screen. Native knows their real heights before
   * React renders them. Nothing when there is no getItemSizeSpec, which turns the
   * feature off on both sides.
   */
  const { rowSizeSpecs, sizeSpecsVersion } = useRowSizeSpecs({
    source,
    getItemSizeSpec,
    mountedIndices,
    lookaheadRows: measureLookaheadRows,
    store,
  });

  /*
   * extraData rebuilds every mounted row when it changes. The rows compare
   * renderItem, and a new one makes them render again.
   */
  const renderItemWithExtraData = useMemo(
    () =>
      extraData === undefined
        ? renderItem
        : (info: Parameters<typeof renderItem>[0]) => renderItem(info),
    [renderItem, extraData]
  );

  const header = useMemo(
    () => renderComponent(ListHeaderComponent),
    [ListHeaderComponent]
  );

  const footer = useMemo(
    () => renderComponent(ListFooterComponent),
    [ListFooterComponent]
  );

  const empty = useMemo(
    () => renderComponent(ListEmptyComponent),
    [ListEmptyComponent]
  );

  const crossPaddingStyle = useMemo(
    () => crossPadding(padding.crossStart, padding.crossEnd, horizontal),
    [horizontal, padding.crossStart, padding.crossEnd]
  );

  /*
   * Content padding goes around the header and footer, like a ScrollView's content container.
   * Their own style then sits on a view inside. Without padding it goes on
   * the template itself and costs no extra view.
   */
  const headerPadding = useMemo(
    () => headerPaddingStyles(crossPaddingStyle, padding.leading, horizontal),
    [crossPaddingStyle, padding.leading, horizontal]
  );

  const footerPadding = useMemo(
    () => footerPaddingStyles(crossPaddingStyle, padding.trailing, horizontal),
    [crossPaddingStyle, padding.trailing, horizontal]
  );

  /*
   * The separator is inside every row. An inline React element would rebuild every mounted
   * row on each caller render. useStableReactElement keeps the previous one while it looks the same.
   * A separator component renders per row with its own props instead.
   */
  const SeparatorComponent = separatorComponentOf(ItemSeparatorComponent);
  const separator = useStableReactElement(
    useMemo(
      () => renderComponent(sharedSeparatorOf(ItemSeparatorComponent)),
      [ItemSeparatorComponent]
    )
  );
  const separatorStoreRef = useRef<SeparatorStore | null>(null);
  if (separatorStoreRef.current === null) {
    separatorStoreRef.current = new SeparatorStore();
  }
  const separatorStore = separatorStoreRef.current;

  /*
   * Rows that stick. A sectioned source has native work them out from its keys, and the prop
   * stays empty. Otherwise they come from the source or the stickyIndices prop.
   */
  const stickyRows = source.nativeSticky
    ? EMPTY_NUMBERS
    : (source.stickyIndices ?? stickyIndices ?? EMPTY_NUMBERS);
  const stickyEnabled = Boolean(
    (source.nativeSticky || stickyRows.length > 0) && renderStickyHeaderOverlay
  );

  const stickyOverlay = useMemo(
    () =>
      stickyEnabled && activeStickyIndex >= 0 && renderStickyHeaderOverlay
        ? renderStickyHeaderOverlay(activeStickyIndex)
        : null,
    [stickyEnabled, activeStickyIndex, renderStickyHeaderOverlay]
  );

  const {
    handleScrollBeginDrag,
    onStartShouldSetResponderCapture,
    onStartShouldSetResponder,
    onResponderRelease,
  } = useKeyboardDismissResponder({
    keyboardDismissMode,
    keyboardShouldPersistTaps,
    onScrollBeginDrag,
    recordInteraction,
  });

  const nativeDecelerationRate =
    typeof decelerationRate === 'number'
      ? decelerationRate
      : decelerationRate
        ? DECELERATION_RATES[decelerationRate]
        : 0;

  useRenderTrace({
    startRef: traceRenderStartRef,
    viewRef: shadowlistViewRef,
    source,
    renderIndices,
    refreshing,
  });

  const viewableEventEnabled =
    viewabilityPairs.length > 0 || stickyEnabled || keySelectionEnabled;
  const columns = cellColumnStyles.length;

  return (
    <ShadowListView
      ref={shadowlistViewRef}
      /*
       * Native reads the header and footer size from their layout frame. In a column they
       * stretch across and a horizontal list would see a screen wide header. A row sizes them
       * by their content instead.
       */
      style={
        autoHeight
          ? [style, styles.containerHorizontal, { height: tallestRowHeight }]
          : [styles.container, style, horizontal && styles.containerHorizontal]
      }
      accessible={accessible}
      accessibilityLabel={accessibilityLabel}
      accessibilityRole={accessibilityRole}
      accessibilityHint={accessibilityHint}
      testID={testID}
      onStartShouldSetResponderCapture={onStartShouldSetResponderCapture}
      onStartShouldSetResponder={onStartShouldSetResponder}
      onResponderRelease={onResponderRelease}
      onMountedRangeChange={handleMountedRangeChange}
      {...mountedRangeProps}
      mountOverscanRows={mountOverscanRows}
      mountOverscanRowsLeading={mountOverscanRowsAhead}
      onViewabilityChange={
        viewableEventEnabled ? handleViewabilityChange : undefined
      }
      prefetchRows={nativePrefetchRows}
      onPrefetchChange={handlePrefetchChange}
      /*
       * An undefined handler drops the JS listener, but the core would still send the event
       * every frame. These flags turn that work off, leaving one event per frame that Fabric
       * can coalesce.
       */
      scrollEventEnabled={onScroll != null}
      scrollEventThrottle={scrollEventThrottle}
      viewableEventEnabled={viewableEventEnabled}
      viewableRules={viewableRules}
      viewableTimings={viewableTimings}
      viewabilityInteracted={viewabilityInteracted}
      visibleRangeEventEnabled={keySelectionEnabled}
      keysStore={store?.id ?? 0}
      keysVersion={source.keysVersion}
      // Codegen wants a string array. Native only reads it. A ReadonlyArray is fine.
      rowKeys={source.propKeys as string[]}
      nonAnchorKeys={(nonAnchorKeys ?? EMPTY_STRINGS) as string[]}
      rowSizeSpecs={rowSizeSpecs}
      sizeSpecsVersion={sizeSpecsVersion}
      inverted={inverted}
      followAppends={followAppends}
      horizontal={horizontal}
      stickyHeader={stickyHeader}
      stickyFooter={stickyFooter}
      autoHideHeader={autoHideHeader}
      autoHideFooter={autoHideFooter}
      stickyIndices={stickyRows as number[]}
      numberOfColumns={numberOfColumns}
      overscan={overscan}
      scrollIndex={scrollIndex}
      refreshEnabled={!!onRefresh}
      refreshing={refreshing}
      refreshColor={refreshColor}
      progressViewOffset={progressViewOffset}
      startReachedThreshold={onStartReachedThreshold}
      endReachedThreshold={onEndReachedThreshold}
      snapToItem={snapToItem}
      snapAlignment={SNAP_ALIGNMENT[snapAlignment]}
      reorderEnabled={reorderEnabled}
      dragBoardId={dragBoardId}
      dragListId={dragListId}
      scrollEnabled={scrollEnabled}
      showsVerticalScrollIndicator={showsVerticalScrollIndicator}
      showsHorizontalScrollIndicator={showsHorizontalScrollIndicator}
      bounces={bounces}
      decelerationRate={nativeDecelerationRate}
      scrollsToTop={scrollsToTop}
      keyboardDismissMode={keyboardDismissMode}
      nestedScrollEnabled={nestedScrollEnabled}
      onStartReached={onStartReached ? handleStartReached : undefined}
      onEndReached={onEndReached ? handleEndReached : undefined}
      onAtEndChange={onAtEndChange ? handleAtEndChange : undefined}
      onScroll={onScroll}
      onScrollBeginDrag={handleScrollBeginDrag}
      onScrollEndDrag={onScrollEndDrag}
      onMomentumScrollBegin={onMomentumScrollBegin}
      onMomentumScrollEnd={onMomentumScrollEnd}
      contentSizeEventEnabled={onContentSizeChange != null}
      onContentSizeChange={
        onContentSizeChange ? handleContentSizeChange : undefined
      }
      onAnchorState={handleAnchorState}
      onRefresh={onRefresh ? handleRefresh : undefined}
      onRefreshSettle={onRefresh ? handleRefreshSettle : undefined}
      onDragStart={handleDragStart}
      onDragEnd={handleDragEnd}
      {...keySelectionProps}
    >
      {Boolean(header || padding.leading > 0 || ListHeaderComponentStyle) && (
        <ShadowListTemplateView
          templateType="header"
          style={headerPadding ?? ListHeaderComponentStyle}
        >
          {headerPadding && ListHeaderComponentStyle ? (
            <View style={ListHeaderComponentStyle}>{header}</View>
          ) : (
            header
          )}
        </ShadowListTemplateView>
      )}
      {rowCount === 0 && empty ? (
        <ShadowListTemplateView templateType="empty" style={crossPaddingStyle}>
          {empty}
        </ShadowListTemplateView>
      ) : (
        renderIndices.map((index) => {
          const item = source.itemAt(index);

          if (!item) return null;

          const rowKey = source.keyAt(index)!;
          const last = index >= rowCount - 1;

          return (
            <CellRenderer
              key={rowKey}
              item={item}
              index={index}
              rowIndex={rowIndex}
              rowKey={rowKey}
              nativeIndex={reorderEnabled ? index : 0}
              style={cellColumnStyles[columns > 1 ? index % columns : 0]}
              renderItem={renderItemWithExtraData}
              separator={last ? null : separator}
              Separator={last ? null : SeparatorComponent}
              trailingItem={
                last || SeparatorComponent === null
                  ? undefined
                  : source.itemAt(index + 1)
              }
              separatorStore={separatorStore}
              selected={selectedKeySet.has(rowKey)}
              selection={selection}
              leadingSwipeActionsForItem={leadingSwipeActionsForItem}
              trailingSwipeActionsForItem={trailingSwipeActionsForItem}
              contextMenuForItem={contextMenuForItem}
              onCellLayout={tracksCells ? handleCellLayout : undefined}
              onCellRelease={tracksCells ? handleCellRelease : undefined}
            />
          );
        })
      )}
      {Boolean(footer || padding.trailing > 0 || ListFooterComponentStyle) && (
        <ShadowListTemplateView
          templateType="footer"
          style={footerPadding ?? ListFooterComponentStyle}
        >
          {footerPadding && ListFooterComponentStyle ? (
            <View style={ListFooterComponentStyle}>{footer}</View>
          ) : (
            footer
          )}
        </ShadowListTemplateView>
      )}
      {stickyEnabled && (
        <ShadowListTemplateView templateType="sectionHeader">
          {stickyOverlay}
        </ShadowListTemplateView>
      )}
    </ShadowListView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  containerHorizontal: {
    flexDirection: 'row',
  },
  cell: {
    position: 'absolute',
  },
  cellVertical: {
    width: '100%',
  },
  cellHorizontal: {
    height: '100%',
  },
});

export const ShadowListBase = forwardRef(ShadowListBaseInner) as <ItemT>(
  props: ShadowListBaseProps<ItemT> & { ref?: Ref<ShadowListCommands> }
) => ReactElement;

/*
 * A list of data. Keys come from keyExtractor, and native gets them through the list's key
 * store, only the ones that changed.
 */
function ShadowListInner<ItemT>(
  { data, keyExtractor = defaultKeyExtractor, ...rest }: ShadowListProps<ItemT>,
  ref: Ref<ShadowListCommands>
) {
  const store = useListStore();
  const source = usePlainSource(data, keyExtractor, store);
  return <ShadowListBase {...rest} ref={ref} source={source} store={store} />;
}

const ShadowList = forwardRef(ShadowListInner) as <ItemT>(
  props: ShadowListProps<ItemT> & { ref?: Ref<ShadowListCommands> }
) => ReactElement;

export default ShadowList;
