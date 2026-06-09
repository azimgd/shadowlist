import type { ComponentRef, Ref } from 'react';
import {
  useState,
  useRef,
  useMemo,
  useEffect,
  useImperativeHandle,
  useCallback,
  startTransition,
  memo,
  forwardRef,
  type ReactElement,
} from 'react';
import { StyleSheet, type CodegenTypes, type ViewStyle } from 'react-native';
import {
  ShadowlistView,
  ShadowlistElementView,
  ShadowlistTemplateView,
  type OnVisibleIndicesChange,
  type OnViewableIndicesChange,
  type OnDragStart,
  type OnDragEnd,
  Commands,
} from 'shadowlist';
import type { ShadowlistProps, ShadowlistCommands, ViewToken } from './types';
import { renderComponent } from './renderComponent';
import { useKeyboardInset } from './useKeyboardInset';
import {
  initialMountedRange,
  rangeToIndices,
  type MountedRange,
} from './virtualization/mountedRange';
import { resolveVisibleIndices } from './virtualization/resolveVisibleIndices';
import { resolveRangeAnchorShift } from './virtualization/anchorShift';
import {
  diffViewableItems,
  resolveActiveStickyIndex,
} from './virtualization/viewableItems';
import { arrayMove, unionDraggedIndex } from './virtualization/dragReorder';

export { initialMountedRange, type MountedRange };

// Default key derivation; module-level so memo/effect deps stay stable.
const defaultKeyExtractor = (item: { id: string }) => item.id;

// Toggle JS-side debug tracing with the [SL] tag.
const SHADOWLIST_DEBUG_LOG = true;

const slLog = (...args: unknown[]) => {
  if (!SHADOWLIST_DEBUG_LOG) return;
  console.log('[SL]', ...args);
};

const SNAP_ALIGNMENT = { start: 0, center: 1, end: 2 } as const;

interface ElementRendererProps<ElementT> {
  element: ElementT;
  index: number;
  style: ViewStyle | ViewStyle[];
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
  separator: ReactElement | null;
}

const ElementRenderer = memo(function ElementRendererInner<
  ElementT extends { id: string },
>({
  element,
  index,
  style,
  renderElement,
  separator,
}: ElementRendererProps<ElementT>) {
  // Memoize on item identity, not row index, so unchanged rows skip re-rendering.
  const children = useMemo(
    () => (
      <>
        {renderElement({ element, index })}
        {separator}
      </>
    ),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [element, renderElement, separator]
  );

  return (
    <ShadowlistElementView index={index} style={style}>
      {children}
    </ShadowlistElementView>
  );
}) as <T extends { id: string }>(
  props: ElementRendererProps<T>
) => ReactElement;

function ShadowlistInner<ElementT extends { id: string }>(
  {
    data: dataProp,
    renderElement,
    keyExtractor = defaultKeyExtractor,
    style,
    elementStyle,
    inverted = false,
    horizontal = false,
    stickyHeader = false,
    stickyFooter = false,
    autoHideHeader = false,
    autoHideFooter = false,
    dragEnabled = false,
    onReorder,
    columns = 1,
    stickyHeaderIndices,
    renderStickyHeaderOverlay,
    containerOffsetIndex = -2,
    keyboardAvoidingEnabled = false,
    keyboardAvoidingOffset = 0,
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
  }: ShadowlistProps<ElementT>,
  ref: Ref<ShadowlistCommands>
) {
  const shadowlistViewRef = useRef<ComponentRef<typeof ShadowlistView> | null>(
    null
  );

  // Keyboard overlap fed to native contentInsetBottom; 0 when disabled.
  const contentInsetBottom = useKeyboardInset(shadowlistViewRef, {
    enabled: keyboardAvoidingEnabled,
    offset: keyboardAvoidingOffset,
  });

  // Pull-to-refresh; consumer drives the spinner via the controlled `refreshing` prop.
  const handleRefresh = useCallback(() => {
    onRefresh?.();
  }, [onRefresh]);

  /*
   * Refresh-prepend deferral: hold data changes that arrive mid-refresh until the
   * spinner has fully retracted (onRefreshSettle), then apply them as an ordinary
   * prepend so MVCP anchors them. Non-refresh changes pass through.
   */
  const refreshDeferEnabled = !!onRefresh && !inverted && !horizontal;
  const [refreshFrozenData, setRefreshFrozenData] =
    useState<ReadonlyArray<ElementT> | null>(null);
  const refreshHoldingRef = useRef(false);
  const data =
    refreshDeferEnabled && refreshHoldingRef.current
      ? (refreshFrozenData ?? dataProp)
      : dataProp;

  useEffect(() => {
    if (!refreshDeferEnabled) {
      refreshHoldingRef.current = false;
      setRefreshFrozenData((prev) => (prev === null ? prev : null));
      return;
    }

    if (refreshing) {
      refreshHoldingRef.current = true;
      setRefreshFrozenData((prev) => prev ?? dataProp);
    }
  }, [dataProp, refreshDeferEnabled, refreshing]);

  // Apply the held refresh-prepend once native reports the spinner has fully retracted.
  const handleRefreshSettle = useCallback(() => {
    refreshHoldingRef.current = false;
    setRefreshFrozenData((prev) => (prev === null ? prev : null));
  }, []);

  // Safety net: release the held prepend shortly after refresh ends in case onRefreshSettle
  // never arrives (e.g. a platform that doesn't emit it). On iOS it fires first, so this is
  // a no-op there.
  const prevRefreshingForFallbackRef = useRef(refreshing);
  useEffect(() => {
    const wasRefreshing = prevRefreshingForFallbackRef.current;
    prevRefreshingForFallbackRef.current = refreshing;
    if (refreshDeferEnabled && wasRefreshing && !refreshing) {
      const timer = setTimeout(handleRefreshSettle, 1200);
      return () => clearTimeout(timer);
    }
    return undefined;
  }, [refreshing, refreshDeferEnabled, handleRefreshSettle]);

  const [mountedRange, setMountedRange] = useState<MountedRange>(() =>
    initialMountedRange(
      data.length,
      initialElementsSize,
      inverted,
      containerOffsetIndex
    )
  );

  /*
   * Mirror of the latest scheduled mounted range (state lags while a transition is
   * pending). Lets the scroll handler decide synchronously and never schedule a no-op
   * update — a no-op transition still re-renders the component once before React bails.
   */
  const mountedRangeRef = useRef<MountedRange>(mountedRange);

  /*
   * Previous visible range, so the overscan can lead in the scroll direction and the
   * mount tier can tell a fling (visible range jumped clear) from an adjacent scroll.
   */
  const prevVisibleRangeRef = useRef<MountedRange>({ low: -1, high: -1 });

  /*
   * Content-anchor the mounted range across data changes (see resolveRangeAnchorShift)
   * in the same render that adopts the new data, so a prepend never blanks the
   * viewport while native's shifted window is still in flight.
   */
  const mountedRangeDataRef = useRef<ReadonlyArray<ElementT>>(data);
  if (mountedRangeDataRef.current !== data) {
    const prevData = mountedRangeDataRef.current;
    mountedRangeDataRef.current = data;
    const shifted = resolveRangeAnchorShift(
      prevData,
      data,
      mountedRange,
      keyExtractor
    );
    if (shifted) {
      slLog(
        'js.rangeAnchor shift',
        `range=[${mountedRange.low}..${mountedRange.high}]->[${shifted.low}..${shifted.high}]`
      );
      mountedRangeRef.current = shifted;
      setMountedRange((prev) =>
        prev.low === shifted.low && prev.high === shifted.high ? prev : shifted
      );
    }
  }

  /*
   * Index of the picked-up row. Force-mounted so it survives virtualization while
   * carried off-screen; the reorder is applied once on drop.
   */
  const [draggingIndex, setDraggingIndex] = useState(-1);

  const elementDimensionStyle = useMemo(() => {
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

  useImperativeHandle(ref, () => ({
    setStartReachedEnabled: (enabled: boolean) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd setStartReachedEnabled', `enabled=${enabled ? 1 : 0}`);
      Commands.setStartReachedEnabled(shadowlistViewRef.current, enabled);
    },
    setEndReachedEnabled: (enabled: boolean) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd setEndReachedEnabled', `enabled=${enabled ? 1 : 0}`);
      Commands.setEndReachedEnabled(shadowlistViewRef.current, enabled);
    },
    scrollToIndex: (index: number) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd scrollToIndex', `index=${index}`);
      Commands.scrollToIndex(shadowlistViewRef.current, index);
    },
    scrollToOffset: (offset: number, animated: boolean = true) => {
      if (!shadowlistViewRef.current) return;

      slLog(
        'js.cmd scrollToOffset',
        `offset=${offset}`,
        `animated=${animated ? 1 : 0}`
      );
      Commands.scrollToOffset(shadowlistViewRef.current, offset, animated);
    },
    scrollToEnd: (animated: boolean = true) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd scrollToEnd', `animated=${animated ? 1 : 0}`);
      Commands.scrollToEnd(shadowlistViewRef.current, animated);
    },
  }));

  /*
   * Flat index of the section header at the top of the viewport (drives the sticky
   * overlay content); -1 when scrolled above the first section header.
   */
  const [activeStickyIndex, setActiveStickyIndex] = useState(-1);

  const updateActiveStickyIndex = useCallback(
    (windowLow: number) => {
      const active = resolveActiveStickyIndex(stickyHeaderIndices, windowLow);
      setActiveStickyIndex((prev) => (prev === active ? prev : active));
    },
    [stickyHeaderIndices]
  );

  const handleVisibleIndicesChange: CodegenTypes.DirectEventHandler<
    OnVisibleIndicesChange,
    never
  > = useCallback(
    (event) => {
      // The policy is pure (see resolveVisibleIndices); this handler owns the refs
      // and turns the decision into React state updates.
      const result = resolveVisibleIndices({
        visibleStartIndex: event.nativeEvent.visibleStartIndex,
        visibleEndIndex: event.nativeEvent.visibleEndIndex,
        prevVisibleRange: prevVisibleRangeRef.current,
        mountedRange: mountedRangeRef.current,
        dataLength: data.length,
      });

      if (result.action === 'ignore') return;
      prevVisibleRangeRef.current = result.visibleRange;

      slLog(
        `js.onVisibleIndicesChange ${result.action}`,
        `visible=[${result.visibleRange.low}..${result.visibleRange.high}]`,
        'band' in result ? `band=[${result.band.low}..${result.band.high}]` : ''
      );

      if (result.action === 'skip') return;

      /*
       * Single funnel for range updates: the band mounts as a transition so growing
       * or trimming the overscan never blocks the active scroll. The ref gate drops
       * updates whose target is already held (or scheduled), so a no-op transition is
       * never queued — queuing one re-renders the component once before React can
       * bail out.
       */
      const next = result.band;
      const scheduled = mountedRangeRef.current;
      if (scheduled.low === next.low && scheduled.high === next.high) return;
      mountedRangeRef.current = next;
      startTransition(() => {
        setMountedRange((prev) =>
          prev.low === next.low && prev.high === next.high ? prev : next
        );
      });
    },
    [data.length]
  );

  const mountedIndices = useMemo(
    () => rangeToIndices(mountedRange),
    [mountedRange]
  );

  // Union the dragged row's index into the rendered set so it stays mounted.
  const renderIndices = useMemo(
    () => unionDraggedIndex(mountedIndices, draggingIndex, data.length),
    [mountedIndices, draggingIndex, data.length]
  );

  const elementsAllKeys = useMemo(
    () => data.map((element, index) => keyExtractor(element, index)),
    [data, keyExtractor]
  );

  // Pickup: keep the picked-up row mounted; data order is unchanged.
  const handleDragStart: CodegenTypes.DirectEventHandler<OnDragStart, never> =
    useCallback((event) => {
      const { index } = event.nativeEvent;
      setDraggingIndex(index);
      slLog('js.onDragStart', `index=${index}`);
    }, []);

  // Drop: apply one array move and hand the result to onReorder.
  const handleDragEnd: CodegenTypes.DirectEventHandler<OnDragEnd, never> =
    useCallback(
      (event) => {
        const { fromIndex, toIndex } = event.nativeEvent;
        setDraggingIndex(-1);
        slLog('js.onDragEnd', `from=${fromIndex}`, `to=${toIndex}`);
        if (fromIndex !== toIndex) {
          onReorder?.({
            from: fromIndex,
            to: toIndex,
            data: arrayMove(data, fromIndex, toIndex),
          });
        }
      },
      [data, onReorder]
    );

  // Release draggingIndex if dragging is disabled mid-gesture (drop event may be lost).
  useEffect(() => {
    if (!dragEnabled) {
      setDraggingIndex((prev) => (prev === -1 ? prev : -1));
    }
  }, [dragEnabled]);

  const prevViewableRef = useRef<ViewToken<ElementT>[]>([]);

  const handleViewableIndicesChange: CodegenTypes.DirectEventHandler<
    OnViewableIndicesChange,
    never
  > = useCallback(
    (event) => {
      const { viewableStartIndex, viewableEndIndex } = event.nativeEvent;

      // Drive sticky-overlay content from the viewable top index to match the pin.
      if (viewableStartIndex !== -1 && viewableEndIndex !== -1) {
        updateActiveStickyIndex(Math.min(viewableStartIndex, viewableEndIndex));
      }

      if (!onViewableItemsChanged) return;

      const { viewableItems, changed } = diffViewableItems(
        data,
        viewableStartIndex,
        viewableEndIndex,
        prevViewableRef.current,
        keyExtractor
      );

      prevViewableRef.current = viewableItems;

      if (changed.length > 0) {
        slLog('js.onViewableItemsChange', `changed=${changed.length}`);
        onViewableItemsChanged({ viewableItems, changed });
      }
    },
    [data, keyExtractor, onViewableItemsChanged, updateActiveStickyIndex]
  );

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

  const separator = useMemo(
    () => renderComponent(ItemSeparatorComponent),
    [ItemSeparatorComponent]
  );

  /*
   * Sticky section-header overlay: one mounted template whose content swaps to the
   * active section's header (null while scrolled above the first header).
   */
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

  slLog('js.render', `indices=[${mountedRange.low}..${mountedRange.high}]`);

  console.log({ mountedRange });

  return (
    <ShadowlistView
      ref={shadowlistViewRef}
      style={[styles.container, style]}
      onVisibleIndicesChange={handleVisibleIndicesChange}
      onViewableIndicesChange={
        onViewableItemsChanged || stickyEnabled
          ? handleViewableIndicesChange
          : undefined
      }
      elementsAllKeys={elementsAllKeys}
      inverted={inverted}
      horizontal={horizontal}
      stickyHeader={stickyHeader}
      stickyFooter={stickyFooter}
      autoHideHeader={autoHideHeader}
      autoHideFooter={autoHideFooter}
      stickyHeaderIndices={stickyHeaderIndices ?? []}
      columns={columns}
      containerOffsetIndex={containerOffsetIndex}
      contentInsetBottom={contentInsetBottom}
      refreshEnabled={!!onRefresh}
      refreshing={refreshing}
      refreshColor={refreshColor}
      startReachedThreshold={onStartReachedThreshold}
      endReachedThreshold={onEndReachedThreshold}
      viewablePercentThreshold={viewablePercentThreshold}
      snapToItem={snapToItem}
      snapToAlignment={SNAP_ALIGNMENT[snapToAlignment]}
      dragEnabled={dragEnabled}
      onStartReached={onStartReached}
      onEndReached={onEndReached}
      onScroll={onScroll}
      onRefresh={onRefresh ? handleRefresh : undefined}
      onRefreshSettle={onRefresh ? handleRefreshSettle : undefined}
      onDragStart={handleDragStart}
      onDragEnd={handleDragEnd}
    >
      {header && (
        <ShadowlistTemplateView templateType="header">
          {header}
        </ShadowlistTemplateView>
      )}
      {data.length === 0 && empty ? (
        <ShadowlistTemplateView templateType="empty">
          {empty}
        </ShadowlistTemplateView>
      ) : (
        renderIndices.map((index) => {
          const element = data[index];

          if (!element) return null;

          return (
            <ElementRenderer
              key={keyExtractor(element, index)}
              element={element}
              index={index}
              style={elementBaseStyle}
              renderElement={renderElement}
              separator={index < data.length - 1 ? separator : null}
            />
          );
        })
      )}
      {footer && (
        <ShadowlistTemplateView templateType="footer">
          {footer}
        </ShadowlistTemplateView>
      )}
      {stickyEnabled && (
        <ShadowlistTemplateView templateType="sectionHeader">
          {stickyOverlay}
        </ShadowlistTemplateView>
      )}
    </ShadowlistView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
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

// Cast preserves the generic element type for callers.
const Shadowlist = forwardRef(ShadowlistInner) as <
  ElementT extends { id: string },
>(
  props: ShadowlistProps<ElementT> & { ref?: Ref<ShadowlistCommands> }
) => ReactElement;

export default Shadowlist;
