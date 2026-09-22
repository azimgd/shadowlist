import {
  useRef,
  useMemo,
  useCallback,
  useLayoutEffect,
  forwardRef,
  type ComponentRef,
  type Ref,
  type ReactElement,
} from 'react';
import { StyleSheet, type ViewStyle } from 'react-native';
import { ShadowListView, ShadowListTemplateView } from 'shadowlist';
import type { ShadowListProps, ShadowListCommands } from './types';
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
  slTrace,
  slTraceEnabled,
  slTraceNow,
  takeRowRenderCount,
  nativeTagOf,
  describeDataChange,
} from './virtualizer';

export { initialMountedRange, type MountedRange } from './virtualizer';

/*
 * Shared empty arrays. An inline [] is a new object on every render, which forces a deep
 * props diff and skips the core's fast path for unchanged keys.
 */
const EMPTY_STRINGS: ReadonlyArray<string> = [];
const EMPTY_NUMBERS: ReadonlyArray<number> = [];

function defaultKeyExtractor(element: { id: string }): string {
  return element.id;
}

function renderComponent(
  component: ReactElement | (() => ReactElement | null) | null | undefined
): ReactElement | null {
  if (!component) return null;
  return typeof component === 'function' ? component() : component;
}

/*
 * The JS side of the native ShadowListView. It mounts only the rows near the screen and
 * passes native scroll, drag and refresh events to the hooks below.
 */
function ShadowListInner<ElementT extends { id: string }>(
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
    dragEnabled = false,
    onReorder,
    columns = 1,
    overscan = 1,
    overscanRows = SHADOWLIST_OVERSCAN,
    overscanRowsLeading = SHADOWLIST_OVERSCAN_LEADING,
    getElementSizeSpec,
    measureLookaheadRows = 48,
    persistentKeys,
    nonAnchorKeys,
    stickyHeaderIndices,
    renderStickyHeaderOverlay,
    containerOffsetIndex = -2,
    trackElementSizes = false,
    refreshing = false,
    onRefresh,
    refreshColor,
    initialElementsSize = 20,
    onStartReached,
    onEndReached,
    onStartReachedThreshold = 1,
    onEndReachedThreshold = 1,
    onScroll,
    snapToItem = false,
    snapToAlignment = 'start',
    viewabilityConfig,
    onViewableItemsChanged,
    ItemSeparatorComponent,
    ListHeaderComponent,
    ListFooterComponent,
    ListEmptyComponent,
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

  const traceRenderStartRef = useRef(0);
  const traceDataRef = useRef<ReadonlyArray<ElementT> | null>(null);
  if (slTraceEnabled()) {
    traceRenderStartRef.current = slTraceNow();
  }

  const handleRefresh = useCallback(() => {
    if (slTraceEnabled()) {
      slTrace(`refresh pull id=${nativeTagOf(shadowlistViewRef.current)}`);
    }
    onRefresh?.();
  }, [onRefresh]);

  /*
   * Wrap the edge reached handlers once, so the trace sees every native call and the
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
   * updates, and the hooks below share them. keyToIndex gives a key's first index, so the
   * first copy of a duplicate wins, same as the core. A plain array can't be diffed
   * cheaply, so both are rebuilt in one pass over the data on every change.
   */
  const { elementsAllKeys, keyToIndex } = useMemo(() => {
    const keys = new Array<string>(data.length);
    const map = new Map<string, number>();
    for (let index = 0; index < data.length; index++) {
      const key = keyExtractor(data[index]!, index);
      keys[index] = key;
      if (!map.has(key)) map.set(key, index);
    }
    return { elementsAllKeys: keys, keyToIndex: map };
  }, [data, keyExtractor]);

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
    dragEnabled,
    onReorder,
  });

  const renderIndices = usePersistentKeys({
    keys: elementsAllKeys,
    persistentKeys,
    renderIndices: draggedIndices,
  });

  const { activeStickyIndex, handleViewableIndicesChange } = useViewability({
    data,
    keys: elementsAllKeys,
    stickyHeaderIndices,
    onViewableItemsChanged,
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
     * Tracking was turned off, so drop the map. The layout callbacks are gone too, and
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

  useImperativeCommands(
    ref,
    shadowlistViewRef,
    elementSizesRef,
    seedAroundIndex
  );

  const elementDimensionStyle = useMemo<ViewStyle>(() => {
    if (horizontal) {
      return columns > 1
        ? { height: `${100 / columns}%` }
        : styles.elementHorizontal;
    } else {
      return columns > 1
        ? { width: `${100 / columns}%` }
        : styles.elementVertical;
    }
  }, [horizontal, columns]);

  const elementBaseStyle = useMemo(
    () =>
      elementStyle
        ? [styles.element, elementDimensionStyle, elementStyle]
        : [styles.element, elementDimensionStyle],
    [elementDimensionStyle, elementStyle]
  );

  /*
   * Precomputed sizes for rows near the screen, so native knows their real heights before
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

  const viewablePercentThreshold =
    (viewabilityConfig?.itemVisiblePercentThreshold ?? 0) / 100;

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

  /*
   * The separator is inside every row, so an inline element would rebuild every mounted
   * row on each caller render. useStableElement keeps the old one while it looks the same.
   */
  const separator = useStableElement(
    useMemo(
      () => renderComponent(ItemSeparatorComponent),
      [ItemSeparatorComponent]
    )
  );

  const stickyEnabled = Boolean(
    stickyHeaderIndices &&
    stickyHeaderIndices.length > 0 &&
    renderStickyHeaderOverlay
  );

  const stickyOverlay = useMemo(
    () =>
      stickyEnabled && activeStickyIndex >= 0 && renderStickyHeaderOverlay
        ? renderStickyHeaderOverlay(activeStickyIndex)
        : null,
    [stickyEnabled, activeStickyIndex, renderStickyHeaderOverlay]
  );

  useLayoutEffect(() => {
    if (!slTraceEnabled()) return;
    const previousData = traceDataRef.current;
    traceDataRef.current = data;
    const first = renderIndices[0] ?? -1;
    const last = renderIndices[renderIndices.length - 1] ?? -1;
    const elapsed = slTraceNow() - traceRenderStartRef.current;
    slTrace(
      `render id=${nativeTagOf(shadowlistViewRef.current)} n=${data.length}` +
        ` mounted=${first}..${last} rows=${takeRowRenderCount()}` +
        ` jsms=${elapsed.toFixed(1)} refreshing=${refreshing ? 1 : 0}` +
        (previousData !== data
          ? ` data=${describeDataChange(previousData, data, keyExtractor)}`
          : '')
    );
  });

  return (
    <ShadowListView
      ref={shadowlistViewRef}
      /*
       * Native reads the header and footer size from their layout frame. In a column they
       * stretch across, so a horizontal list would see a screen wide header. A row sizes them
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
      onVisibleIndicesChange={handleVisibleIndicesChange}
      onViewableIndicesChange={
        onViewableItemsChanged || stickyEnabled
          ? handleViewableIndicesChange
          : undefined
      }
      /*
       * An undefined handler drops the JS listener, but the core would still send the event
       * every frame. These flags turn that work off, leaving one event per frame that Fabric
       * can coalesce.
       */
      scrollEventEnabled={onScroll != null}
      viewableEventEnabled={onViewableItemsChanged != null || stickyEnabled}
      elementsAllKeys={elementsAllKeys}
      // Codegen wants a string array. Native only reads it, so a ReadonlyArray is fine.
      elementsAnchorIgnoreKeys={(nonAnchorKeys ?? EMPTY_STRINGS) as string[]}
      elementsSizeSpecs={elementsSizeSpecs}
      inverted={inverted}
      followAppends={followAppends}
      horizontal={horizontal}
      stickyHeader={stickyHeader}
      stickyFooter={stickyFooter}
      autoHideHeader={autoHideHeader}
      autoHideFooter={autoHideFooter}
      stickyHeaderIndices={stickyHeaderIndices ?? EMPTY_NUMBERS}
      columns={columns}
      overscan={overscan}
      containerOffsetIndex={containerOffsetIndex}
      refreshEnabled={!!onRefresh}
      refreshing={refreshing}
      refreshColor={refreshColor}
      startReachedThreshold={onStartReachedThreshold}
      endReachedThreshold={onEndReachedThreshold}
      viewablePercentThreshold={viewablePercentThreshold}
      snapToItem={snapToItem}
      snapToAlignment={SNAP_ALIGNMENT[snapToAlignment]}
      dragEnabled={dragEnabled}
      onStartReached={onStartReached ? handleStartReached : undefined}
      onEndReached={onEndReached ? handleEndReached : undefined}
      onScroll={onScroll}
      onRefresh={onRefresh ? handleRefresh : undefined}
      onRefreshSettle={onRefresh ? handleRefreshSettle : undefined}
      onDragStart={handleDragStart}
      onDragEnd={handleDragEnd}
    >
      {header && (
        <ShadowListTemplateView templateType="header">
          {header}
        </ShadowListTemplateView>
      )}
      {data.length === 0 && empty ? (
        <ShadowListTemplateView templateType="empty">
          {empty}
        </ShadowListTemplateView>
      ) : (
        renderIndices.map((index) => {
          const element = data[index];

          if (!element) return null;

          const elementKey = elementsAllKeys[index]!;

          return (
            <ElementRenderer
              key={elementKey}
              element={element}
              index={index}
              elementKey={elementKey}
              nativeIndex={dragEnabled ? index : 0}
              style={elementBaseStyle}
              renderElement={renderElement}
              separator={index < data.length - 1 ? separator : null}
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
      {footer && (
        <ShadowListTemplateView templateType="footer">
          {footer}
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

const ShadowList = forwardRef(ShadowListInner) as <
  ElementT extends { id: string },
>(
  props: ShadowListProps<ElementT> & { ref?: Ref<ShadowListCommands> }
) => ReactElement;

export default ShadowList;
