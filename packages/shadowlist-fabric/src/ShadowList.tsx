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
 * Stable empty arrays. An inline `[]` is a new identity on every render, which makes the
 * props object differ, which costs a deep prop diff and defeats the core's props-identity
 * shortcut for the key collection.
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
 * The JS layer over the native <ShadowListView>. It narrows the full `data` prop down
 * to the rows visible in the viewport, and routes the native scroll/drag/refresh events
 * into the hooks below.
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
    getElementSizeSpec,
    measureLookaheadRows = 48,
    persistentKeys,
    nonAnchorKeys,
    stickyHeaderIndices,
    renderStickyHeaderOverlay,
    containerOffsetIndex = -2,
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
   * Edge-reached handlers go through a stable wrapper so the trace sees every native fire
   * without the handler identity following the caller's.
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
   * While a refresh runs, hold back incoming data until the spinner finishes retracting
   * so rows don't jump under the user. `data` is the list we actually render.
   */
  const { data, handleRefreshSettle } = useRefreshDefer({
    data: dataProp,
    refreshing,
    onRefresh,
    inverted,
    horizontal,
  });

  /*
   * Every row's key, extracted once per data change: handed to native so it can track row
   * identity across data updates, and shared by the hooks below. keyToIndex resolves a key
   * to its first index (first occurrence wins on a duplicate, matching the core's
   * reconcile). A plain array gives no cheap diff signal, so both are rebuilt in full on
   * every data change -- one O(N) pass, not one per row.
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

  const { mountedIndices, handleVisibleIndicesChange } = useMountedRange({
    keys: elementsAllKeys,
    keyToIndex,
    initialElementsSize,
    inverted,
    followAppends,
    containerOffsetIndex,
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

  useImperativeCommands(ref, shadowlistViewRef);

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
   * Ahead-of-time row sizes for the rows around the viewport, so native knows their real
   * heights before React renders them (see ShadowListProps.getElementSizeSpec). '' when the
   * caller supplied no getElementSizeSpec, which disables the feature on both sides.
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
   * The separator is part of every row's content, so a caller writing it inline
   * (`ItemSeparatorComponent={<Separator />}`) would otherwise rebuild the whole mounted
   * window on each of its own renders; useStableElement keeps the element while it describes
   * the same thing.
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
       * Native reads each header/footer template's scroll-axis size from its Yoga frame. In a
       * column the templates stretch across the width, so a horizontal list would measure the
       * header as viewport-wide; a row sizes them by content along the scroll axis instead.
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
       * Passing `undefined` for a handler removes the JS listener but not the native work:
       * the core still computes and dispatches the event every frame. These tell it not to
       * bother, and keep the per-frame event stream down to the one event that always has
       * a listener, which is what lets Fabric coalesce it (see the native component spec).
       */
      scrollEventEnabled={onScroll != null}
      viewableEventEnabled={onViewableItemsChanged != null || stickyEnabled}
      elementsAllKeys={elementsAllKeys}
      /*
       * The codegen spec types this `string[]`; native only reads it, so the public prop can
       * stay a ReadonlyArray.
       */
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
