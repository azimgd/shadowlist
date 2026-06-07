import type { ComponentRef, Ref } from 'react';
import {
  useState,
  useRef,
  useMemo,
  useEffect,
  useImperativeHandle,
  useCallback,
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
import { useKeyboardInset } from './useKeyboardInset';

// Move the item at `from` to `to`, returning a new array.
const arrayMove = <T,>(
  input: ReadonlyArray<T>,
  from: number,
  to: number
): T[] => {
  const next = input.slice();
  if (
    from < 0 ||
    from >= next.length ||
    to < 0 ||
    to >= next.length ||
    from === to
  ) {
    return next;
  }
  const moved = next.splice(from, 1)[0] as T;
  next.splice(to, 0, moved);
  return next;
};

// Default key derivation; module-level so memo/effect deps stay stable.
const defaultKeyExtractor = (item: { id: string }) => item.id;

// Toggle JS-side debug tracing with the [SL] tag.
const SHADOWLIST_DEBUG_LOG = false;

const slLog = (...args: unknown[]) => {
  if (!SHADOWLIST_DEBUG_LOG) return;
  console.log('[SL]', ...args);
};

/*
 * Mounted range [low, high]. Mounts SHADOWLIST_OVERSCAN extra rows on each side of
 * the visible window and only re-renders when the window leaves the mounted range.
 */
export interface MountedRange {
  low: number;
  high: number;
}

const SHADOWLIST_OVERSCAN = 4;

export const initialMountedRange = (
  size: number,
  initial: number,
  inverted: boolean,
  offsetIndex: number
): MountedRange => {
  if (size <= 0) return { low: -1, high: -1 };
  /*
   * With an explicit initial target, seed the range around it (avoids a blank flash
   * at the target). An explicit target overrides the inverted bottom anchor.
   */
  if (offsetIndex >= 0) {
    const target = Math.min(offsetIndex, size - 1);
    return {
      low: Math.max(0, target - SHADOWLIST_OVERSCAN),
      high: Math.min(size - 1, target + initial),
    };
  }
  if (inverted) {
    return { low: Math.max(0, size - initial), high: size - 1 };
  }
  return { low: 0, high: Math.min(initial, size - 1) };
};

const rangeToIndices = (range: MountedRange): number[] => {
  if (range.low < 0 || range.high < 0 || range.low > range.high) return [];
  const indices: number[] = [];
  for (let index = range.low; index <= range.high; index++) indices.push(index);
  return indices;
};

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
  const [committedData, setCommittedData] =
    useState<ReadonlyArray<ElementT>>(dataProp);
  const refreshHoldingRef = useRef(false);
  const refreshHeldDataRef = useRef<ReadonlyArray<ElementT> | null>(null);
  const prevRefreshingRef = useRef(refreshing);
  if (refreshDeferEnabled && !prevRefreshingRef.current && refreshing) {
    // A refresh just started: hold subsequent data changes until it settles.
    refreshHoldingRef.current = true;
  }
  prevRefreshingRef.current = refreshing;
  if (dataProp !== committedData) {
    if (refreshHoldingRef.current) {
      refreshHeldDataRef.current = dataProp; // keep rendering pre-refresh data until settle
    } else {
      setCommittedData(dataProp);
    }
  }
  const data = committedData;

  // Apply the held refresh-prepend once native reports the spinner has fully retracted.
  const handleRefreshSettle = useCallback(() => {
    refreshHoldingRef.current = false;
    if (refreshHeldDataRef.current !== null) {
      setCommittedData(refreshHeldDataRef.current);
      refreshHeldDataRef.current = null;
    }
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
   * Set when a mounted-range anchor shift just happened, so the next native
   * onVisibleIndicesChange re-centers (tightens) the range to its window, not skip it.
   */
  const tightenAfterShiftRef = useRef(false);

  /*
   * Content-anchor the mounted range across data changes. On a prepend above the
   * viewport, native shifts its window by +N but only reports it after an async
   * round-trip; until then a stale range blanks the viewport. So here, in the same
   * render that adopts the new data, find the new index of the element at range.low
   * and shift the range by that delta. The shift is a union (never shrunk) so the
   * swept-over rows stay mounted; tightenAfterShiftRef makes the next native emit
   * re-center it back to window+overscan. Mid-range inserts fall back to the round-trip.
   */
  const [mountedRangeData, setMountedRangeData] =
    useState<ReadonlyArray<ElementT>>(data);
  if (mountedRangeData !== data) {
    setMountedRangeData(data);
    if (mountedRange.low >= 0 && mountedRange.low < mountedRangeData.length) {
      const anchorKey = keyExtractor(
        mountedRangeData[mountedRange.low]!,
        mountedRange.low
      );
      let newLow = -1;
      for (let index = 0; index < data.length; index++) {
        if (keyExtractor(data[index]!, index) === anchorKey) {
          newLow = index;
          break;
        }
      }
      if (newLow >= 0 && newLow !== mountedRange.low) {
        const delta = newLow - mountedRange.low;
        const maxIndex = data.length - 1;
        const clamp = (value: number) => Math.min(Math.max(0, value), maxIndex);
        const low = clamp(Math.min(mountedRange.low, mountedRange.low + delta));
        const high = clamp(
          Math.max(mountedRange.high, mountedRange.high + delta)
        );
        tightenAfterShiftRef.current = true;
        slLog(
          'js.rangeAnchor shift',
          `delta=${delta}`,
          `range=[${mountedRange.low}..${mountedRange.high}]->[${low}..${high}]`
        );
        setMountedRange((prev) =>
          prev.low === low && prev.high === high ? prev : { low, high }
        );
      }
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
      if (!stickyHeaderIndices || stickyHeaderIndices.length === 0) {
        setActiveStickyIndex((prev) => (prev === -1 ? prev : -1));
        return;
      }
      let active = -1;
      for (const stickyIndex of stickyHeaderIndices) {
        if (stickyIndex <= windowLow) active = stickyIndex;
        else break;
      }
      setActiveStickyIndex((prev) => (prev === active ? prev : active));
    },
    [stickyHeaderIndices]
  );

  const handleVisibleIndicesChange: CodegenTypes.DirectEventHandler<
    OnVisibleIndicesChange,
    never
  > = useCallback(
    (event) => {
      const { visibleStartIndex, visibleEndIndex } = event.nativeEvent;
      if (visibleStartIndex === -1 || visibleEndIndex === -1) return;
      // Normalise to an ascending window (inverted lists report start > end).
      const windowLow = Math.min(visibleStartIndex, visibleEndIndex);
      const windowHigh = Math.max(visibleStartIndex, visibleEndIndex);

      // First native window after a mounted-range anchor shift: re-center the range on
      // it (window + overscan), even if it is already inside the unioned shift range. This
      // returns the range to its normal size so repeated prepends do not grow it.
      if (tightenAfterShiftRef.current) {
        tightenAfterShiftRef.current = false;
        const low = Math.max(0, windowLow - SHADOWLIST_OVERSCAN);
        const high = Math.min(
          data.length - 1,
          windowHigh + SHADOWLIST_OVERSCAN
        );
        slLog(
          'js.onVisibleIndicesChange tighten',
          `window=[${windowLow}..${windowHigh}]`,
          `range=[${low}..${high}]`
        );
        setMountedRange((prev) =>
          prev.low === low && prev.high === high ? prev : { low, high }
        );
        return;
      }

      setMountedRange((prevRange) => {
        // Window already inside the range; rows are mounted, skip the re-render.
        if (
          prevRange.low >= 0 &&
          windowLow >= prevRange.low &&
          windowHigh <= prevRange.high
        ) {
          slLog(
            'js.onVisibleIndicesChange skip (in range)',
            `window=[${windowLow}..${windowHigh}]`,
            `range=[${prevRange.low}..${prevRange.high}]`
          );
          return prevRange;
        }
        const low = Math.max(0, windowLow - SHADOWLIST_OVERSCAN);
        const high = Math.min(
          data.length - 1,
          windowHigh + SHADOWLIST_OVERSCAN
        );
        slLog(
          'js.onVisibleIndicesChange apply',
          `window=[${windowLow}..${windowHigh}]`,
          `range=[${low}..${high}]`
        );
        return { low, high };
      });
    },
    [data.length]
  );

  const mountedIndices = useMemo(
    () => rangeToIndices(mountedRange),
    [mountedRange]
  );

  // Union the dragged row's index into the rendered set so it stays mounted.
  const renderIndices = useMemo(() => {
    if (
      draggingIndex < 0 ||
      draggingIndex >= data.length ||
      mountedIndices.includes(draggingIndex)
    ) {
      return mountedIndices;
    }
    return [...mountedIndices, draggingIndex].sort((a, b) => a - b);
  }, [mountedIndices, draggingIndex, data.length]);

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
      // Normalise to an ascending range (inverted lists report higher index first).
      const lo = Math.min(viewableStartIndex, viewableEndIndex);
      const hi = Math.max(viewableStartIndex, viewableEndIndex);

      // Drive sticky-overlay content from the viewable top index to match the pin.
      if (viewableStartIndex !== -1 && viewableEndIndex !== -1) {
        updateActiveStickyIndex(lo);
      }

      if (!onViewableItemsChanged) return;

      const viewableItems: ViewToken<ElementT>[] = [];
      if (viewableStartIndex !== -1 && viewableEndIndex !== -1) {
        for (let index = lo; index <= hi; index++) {
          const item = data[index];
          if (!item) continue;
          viewableItems.push({
            item,
            index,
            key: keyExtractor(item, index),
            isViewable: true,
          });
        }
      }

      const currentKeys = new Set(viewableItems.map((token) => token.key));
      const prevKeys = new Set(
        prevViewableRef.current.map((token) => token.key)
      );

      const changed: ViewToken<ElementT>[] = [
        ...viewableItems.filter((token) => !prevKeys.has(token.key)),
        ...prevViewableRef.current
          .filter((token) => !currentKeys.has(token.key))
          .map((token) => ({ ...token, isViewable: false })),
      ];

      prevViewableRef.current = viewableItems;

      if (changed.length > 0) {
        slLog(
          'js.onViewableItemsChange',
          `viewable=[${lo}..${hi}]`,
          `changed=${changed.length}`
        );
        onViewableItemsChanged({ viewableItems, changed });
      }
    },
    [data, keyExtractor, onViewableItemsChanged, updateActiveStickyIndex]
  );

  const viewablePercentThreshold =
    (viewabilityConfig?.itemVisiblePercentThreshold ?? 0) / 100;

  const renderComponent = (
    component: ReactElement | (() => ReactElement | null) | null | undefined
  ): ReactElement | null => {
    if (!component) return null;
    return typeof component === 'function' ? component() : component;
  };

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
