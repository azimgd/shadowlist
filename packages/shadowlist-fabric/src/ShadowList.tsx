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
  type OnContentSizeChange,
} from './ShadowListViewNativeComponent';
import ShadowListTemplateView from './ShadowListTemplateViewNativeComponent';
import type {
  ShadowListProps,
  ShadowListCommands,
  ViewabilityConfigCallbackPair,
} from './types';
import {
  ElementRenderer,
  SNAP_ALIGNMENT,
  SHADOWLIST_OVERSCAN,
  SHADOWLIST_OVERSCAN_LEADING,
  useMountedRange,
  useRefreshDefer,
  useDragReorder,
  usePersistentKeys,
  useViewability,
  useImperativeCommands,
  useElementSizeSpecs,
  useStableElement,
  useRowSelection,
  usePrefetch,
  useAnchorState,
  useKeyboardDismissResponder,
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
import { selectedIndices } from './virtualizer/selection';
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
 * props diff and skips the core's fast path for unchanged keys.
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
 * The JS side of the native ShadowListView. It mounts only the rows near the screen and
 * passes native scroll, drag and refresh events to the hooks below.
 */
function ShadowListInner<ElementT>(
  {
    data: dataProp,
    renderElement,
    keyExtractor = defaultKeyExtractor,
    style,
    elementStyle,
    inverted = false,
    followAppends = false,
    horizontal = false,
    stickyHeader = false,
    stickyFooter = false,
    autoHideHeader = false,
    autoHideFooter = false,
    reorderEnabled = false,
    onReorder,
    numberOfColumns = 1,
    overscan = 1,
    overscanRows = SHADOWLIST_OVERSCAN,
    overscanRowsLeading = SHADOWLIST_OVERSCAN_LEADING,
    getElementSizeSpec,
    measureLookaheadRows = 48,
    persistentKeys,
    nonAnchorKeys,
    stickyIndices,
    renderStickyHeaderOverlay,
    initialScrollIndex,
    containerOffsetIndex: containerOffsetIndexProp,
    trackElementSizes = false,
    extraData,
    refreshing = false,
    onRefresh,
    refreshColor,
    progressViewOffset = 0,
    initialElementsSize = 20,
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
  }: ShadowListProps<ElementT>,
  ref: Ref<ShadowListCommands>
) {
  const shadowlistViewRef = useRef<ComponentRef<typeof ShadowListView> | null>(
    null
  );

  const traceRenderStartRef = useRenderTraceStart();

  /*
   * initialScrollIndex only counts on mount. containerOffsetIndex scrolls
   * whenever it changes and wins when both are set.
   */
  const [initialIndex] = useState(initialScrollIndex);
  const containerOffsetIndex =
    containerOffsetIndexProp ??
    (initialIndex != null && initialIndex >= 0 ? initialIndex : -2);

  const handleRefresh = useCallback(() => {
    if (slTraceEnabled()) {
      slTrace(`refresh pull id=${nativeTagOf(shadowlistViewRef.current)}`);
    }
    onRefresh?.();
  }, [onRefresh]);

  /*
   * Wrap the edge reached handlers once. The trace then sees every native call and the
   * handler doesn't change whenever the caller's does.
   */
  const edgeHandlersRef = useRef({ onStartReached, onEndReached });
  edgeHandlersRef.current = { onStartReached, onEndReached };
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

  /*
   * During a refresh, hold new data until the spinner is gone so rows don't jump under
   * the user. data is what we actually render.
   */
  const { data, handleRefreshSettle } = useRefreshDefer({
    data: dataProp,
    refreshing,
    onRefresh,
    inverted,
    horizontal,
  });

  /*
   * Every row's key, built once per data change. Native uses them to follow rows across
   * updates, and the hooks below share them. keyToIndex gives a key's first index. The
   * first copy of a duplicate wins, same as the core.
   * When the keys come out the same as last time, like an item edited in place, the previous
   * array and map are returned. Then React sends no new elementsAllKeys prop to native (no
   * deep compare, no conversion of every key) and the hooks below keyed on them don't rerun.
   */
  const keysCacheRef = useRef<{
    elementsAllKeys: string[];
    keyToIndex: Map<string, number>;
  } | null>(null);
  const { elementsAllKeys, keyToIndex } = useMemo(() => {
    const previous = keysCacheRef.current;
    const keys = new Array<string>(data.length);
    let same =
      previous !== null && previous.elementsAllKeys.length === data.length;
    for (let index = 0; index < data.length; index++) {
      const key = keyExtractor(data[index]!, index);
      keys[index] = key;
      if (same && previous!.elementsAllKeys[index] !== key) same = false;
    }
    if (same) return previous!;
    const map = new Map<string, number>();
    for (let index = 0; index < keys.length; index++) {
      const key = keys[index]!;
      if (!map.has(key)) map.set(key, index);
    }
    const next = { elementsAllKeys: keys, keyToIndex: map };
    keysCacheRef.current = next;
    return next;
  }, [data, keyExtractor]);

  /*
   * Shared with every row. A row that skipped a move still answers a late index read.
   * Written in render, before the rows render, like each row's own index.
   */
  const rowIndexRef = useRef<RowIndexStore | null>(null);
  if (rowIndexRef.current === null) rowIndexRef.current = createRowIndexStore();
  const rowIndex = rowIndexRef.current;
  rowIndex.keyToIndex = keyToIndex;
  rowIndex.keys = elementsAllKeys;

  const { mountedIndices, handleVisibleIndicesChange, seedAroundIndex } =
    useMountedRange({
      keys: elementsAllKeys,
      keyToIndex,
      initialElementsSize,
      inverted,
      followAppends,
      containerOffsetIndex,
      overscanRows,
      // Never mount fewer rows ahead during a fling than at rest, that's where rows are needed most.
      overscanRowsLeading: Math.max(overscanRows, overscanRowsLeading),
    });

  const {
    renderIndices: draggedIndices,
    handleDragStart,
    handleDragEnd,
  } = useDragReorder({
    data,
    keyToIndex,
    mountedIndices,
    reorderEnabled,
    onReorder,
  });

  const renderIndices = usePersistentKeys({
    keys: elementsAllKeys,
    persistentKeys,
    renderIndices: draggedIndices,
  });

  /*
   * One viewability pair per config, the single config and callback included.
   */
  const viewabilityPairs = useMemo<
    ReadonlyArray<ViewabilityConfigCallbackPair<ElementT>>
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
    handleViewableIndicesChange,
    recordInteraction,
  } = useViewability({
    data,
    keys: elementsAllKeys,
    stickyIndices,
    pairs: viewabilityPairs,
  });

  /*
   * Row sizes by key when trackElementSizes is on, null when off. A ref, since only
   * imperative calls read it.
   */
  const elementSizesRef = useRef<Map<string, number> | null>(null);
  if (trackElementSizes) {
    if (elementSizesRef.current === null) elementSizesRef.current = new Map();
  } else if (elementSizesRef.current !== null) {
    /*
     * Tracking was turned off. Drop the map. The layout callbacks are gone too, and
     * nothing would keep it up to date.
     */
    elementSizesRef.current = null;
  }

  const handleElementLayout = useCallback(
    (key: string, width: number, height: number) => {
      elementSizesRef.current?.set(key, horizontal ? width : height);
    },
    [horizontal]
  );

  const handleElementRelease = useCallback((key: string) => {
    elementSizesRef.current?.delete(key);
  }, []);

  const { selectedKeySet, selection, selectionStateRef } = useRowSelection({
    keyToIndex,
    selectedKeys,
    allowsMultipleSelection,
    onSelectionChange,
  });

  usePrefetch({
    keys: elementsAllKeys,
    keyToIndex,
    mountedIndices,
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
      rowIndex,
      keyToIndex,
      seedAroundIndex,
    });

  const commandSourceRef = useRef<CommandSource | null>(null);
  const commandSource: CommandSource = {
    data,
    keyToIndex,
    highestMountedIndex: mountedHigh,
    contentLength: contentLengthRef.current,
    onScrollToIndexFailed,
    seedAroundIndex,
    recordInteraction,
    selectIndex: (index: number) => {
      const key = elementsAllKeys[index];
      if (key !== undefined) selection.select(key);
    },
    deselectIndex: (index: number) => {
      const key = elementsAllKeys[index];
      if (key !== undefined) selection.deselect(key);
    },
    getSelectedIndices: () =>
      selectedIndices(selectionStateRef.current.keys, rowIndex.keyToIndex),
    requestAnchorState,
    restoreAnchorState,
  };
  commandSourceRef.current = commandSource;

  useImperativeCommands(
    ref,
    shadowlistViewRef,
    elementSizesRef,
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

  const elementDimensionStyle = useMemo<ViewStyle>(() => {
    if (horizontal) {
      return numberOfColumns > 1
        ? { height: `${100 / numberOfColumns}%` }
        : styles.elementHorizontal;
    } else {
      return numberOfColumns > 1
        ? { width: `${100 / numberOfColumns}%` }
        : styles.elementVertical;
    }
  }, [horizontal, numberOfColumns]);

  /*
   * One row style per column. Columns differ only when they carry padding.
   */
  const elementColumnStyles = useMemo<StyleProp<ViewStyle>[]>(() => {
    const base: StyleProp<ViewStyle>[] = elementStyle
      ? [styles.element, elementDimensionStyle, elementStyle]
      : [styles.element, elementDimensionStyle];
    if (!columnPaddings) return [base];
    return columnPaddings.map((paddingStyle) => [...base, paddingStyle]);
  }, [elementDimensionStyle, elementStyle, columnPaddings]);

  /*
   * Precomputed sizes for rows near the screen. Native knows their real heights before
   * React renders them. Empty string when there is no getElementSizeSpec, which turns the
   * feature off on both sides.
   */
  const elementsSizeSpecs = useElementSizeSpecs({
    data,
    keys: elementsAllKeys,
    getElementSizeSpec,
    mountedIndices,
    lookaheadRows: measureLookaheadRows,
  });

  /*
   * extraData rebuilds every mounted row when it changes. The rows compare
   * renderElement, and a new one makes them render again.
   */
  const renderElementWithExtraData = useMemo(
    () =>
      extraData === undefined
        ? renderElement
        : (info: Parameters<typeof renderElement>[0]) => renderElement(info),
    [renderElement, extraData]
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
   * The separator is inside every row. An inline element would rebuild every mounted
   * row on each caller render. useStableElement keeps the old one while it looks the same.
   * A separator component renders per row with its own props instead.
   */
  const SeparatorComponent = separatorComponentOf(ItemSeparatorComponent);
  const separator = useStableElement(
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

  const stickyEnabled = Boolean(
    stickyIndices && stickyIndices.length > 0 && renderStickyHeaderOverlay
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
    data,
    renderIndices,
    refreshing,
    keyExtractor,
  });

  const viewableEventEnabled = viewabilityPairs.length > 0 || stickyEnabled;
  const columns = elementColumnStyles.length;

  return (
    <ShadowListView
      ref={shadowlistViewRef}
      /*
       * Native reads the header and footer size from their layout frame. In a column they
       * stretch across and a horizontal list would see a screen wide header. A row sizes them
       * by their content instead.
       */
      style={[
        styles.container,
        style,
        horizontal && styles.containerHorizontal,
      ]}
      accessible={accessible}
      accessibilityLabel={accessibilityLabel}
      accessibilityRole={accessibilityRole}
      accessibilityHint={accessibilityHint}
      testID={testID}
      onStartShouldSetResponderCapture={onStartShouldSetResponderCapture}
      onStartShouldSetResponder={onStartShouldSetResponder}
      onResponderRelease={onResponderRelease}
      onVisibleIndicesChange={handleVisibleIndicesChange}
      onViewableIndicesChange={
        viewableEventEnabled ? handleViewableIndicesChange : undefined
      }
      /*
       * An undefined handler drops the JS listener, but the core would still send the event
       * every frame. These flags turn that work off, leaving one event per frame that Fabric
       * can coalesce.
       */
      scrollEventEnabled={onScroll != null}
      scrollEventThrottle={scrollEventThrottle}
      viewableEventEnabled={viewableEventEnabled}
      viewableRules={viewableRules}
      elementsAllKeys={elementsAllKeys}
      // Codegen wants a string array. Native only reads it. A ReadonlyArray is fine.
      elementsAnchorIgnoreKeys={(nonAnchorKeys ?? EMPTY_STRINGS) as string[]}
      elementsSizeSpecs={elementsSizeSpecs}
      inverted={inverted}
      followAppends={followAppends}
      horizontal={horizontal}
      stickyHeader={stickyHeader}
      stickyFooter={stickyFooter}
      autoHideHeader={autoHideHeader}
      autoHideFooter={autoHideFooter}
      stickyIndices={stickyIndices ?? EMPTY_NUMBERS}
      numberOfColumns={numberOfColumns}
      overscan={overscan}
      containerOffsetIndex={containerOffsetIndex}
      refreshEnabled={!!onRefresh}
      refreshing={refreshing}
      refreshColor={refreshColor}
      refreshProgressViewOffset={progressViewOffset}
      startReachedThreshold={onStartReachedThreshold}
      endReachedThreshold={onEndReachedThreshold}
      snapToItem={snapToItem}
      snapAlignment={SNAP_ALIGNMENT[snapAlignment]}
      reorderEnabled={reorderEnabled}
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
      {data.length === 0 && empty ? (
        <ShadowListTemplateView templateType="empty" style={crossPaddingStyle}>
          {empty}
        </ShadowListTemplateView>
      ) : (
        renderIndices.map((index) => {
          const element = data[index];

          if (!element) return null;

          const elementKey = elementsAllKeys[index]!;
          const last = index >= data.length - 1;

          return (
            <ElementRenderer
              key={elementKey}
              element={element}
              index={index}
              rowIndex={rowIndex}
              elementKey={elementKey}
              nativeIndex={reorderEnabled ? index : 0}
              style={elementColumnStyles[columns > 1 ? index % columns : 0]}
              renderElement={renderElementWithExtraData}
              separator={last ? null : separator}
              Separator={last ? null : SeparatorComponent}
              trailingElement={
                last || SeparatorComponent === null
                  ? undefined
                  : data[index + 1]
              }
              separatorStore={separatorStore}
              selected={selectedKeySet.has(elementKey)}
              selection={selection}
              leadingSwipeActionsForItem={leadingSwipeActionsForItem}
              trailingSwipeActionsForItem={trailingSwipeActionsForItem}
              contextMenuForItem={contextMenuForItem}
              onElementLayout={
                trackElementSizes ? handleElementLayout : undefined
              }
              onElementRelease={
                trackElementSizes ? handleElementRelease : undefined
              }
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
  element: {
    position: 'absolute',
  },
  elementVertical: {
    width: '100%',
  },
  elementHorizontal: {
    height: '100%',
  },
});

const ShadowList = forwardRef(ShadowListInner) as <ElementT>(
  props: ShadowListProps<ElementT> & { ref?: Ref<ShadowListCommands> }
) => ReactElement;

export default ShadowList;
