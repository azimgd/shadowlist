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

/*
 * Move the item at `from` to `to`, returning a new array. Used once on drop to
 * produce the reordered array handed to onReorder.
 */
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

/*
 * Stable default key derivation (element.id), kept module-level so the memo/effect
 * dependencies stay referentially stable when no keyExtractor is supplied.
 */
const defaultKeyExtractor = (item: { id: string }) => item.id;

/*
 * Trace the JS <-> native state synchronization. Mirrors the SHADOWLIST_DEBUG_LOG
 * flag in the C++ core (shadowlist-core/Constants.hpp) and shares the same [SL]
 * tag, so the JS, C++ and iOS/Android logs interleave into one readable stream.
 * Only the meaningful boundaries are logged (native events applied upward,
 * imperative commands sent downward) to keep it off the per-frame scroll path.
 */
const SHADOWLIST_DEBUG_LOG = false;

const slLog = (...args: unknown[]) => {
  if (!SHADOWLIST_DEBUG_LOG) return;
  console.log('[SL]', ...args);
};

/*
 * Mounted band [low, high] (low/high-water mark). The core reports a buffered
 * visible window; we mount SHADOWLIST_OVERSCAN extra rows on each side and only
 * re-render to grow/shift the band when the window leaves it. Windows are
 * normalised to ascending, so inverted lists (start > end) use the same path.
 */
export interface VisibleBand {
  low: number;
  high: number;
}

const SHADOWLIST_OVERSCAN = 4;

export const initialBand = (
  size: number,
  initial: number,
  inverted: boolean
): VisibleBand => {
  if (size <= 0) return { low: -1, high: -1 };
  if (inverted) {
    return { low: Math.max(0, size - initial), high: size - 1 };
  }
  return { low: 0, high: Math.min(initial, size - 1) };
};

const bandToRange = (band: VisibleBand): number[] => {
  if (band.low < 0 || band.high < 0 || band.low > band.high) return [];
  const range: number[] = [];
  for (let index = band.low; index <= band.high; index++) range.push(index);
  return range;
};

interface ElementRendererProps<ElementT> {
  element: ElementT;
  index: number;
  style: ViewStyle | ViewStyle[];
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
  separator: ReactElement | null;
}

const ElementRenderer = memo(function ElementRenderer<
  ElementT extends { id: string },
>({
  element,
  index,
  style,
  renderElement,
  separator,
}: ElementRendererProps<ElementT>) {
  /*
   * Memoize on the item identity, not the row index. A prepend (or any insertion
   * above this row) shifts every row's index, so keying on `element` lets unchanged
   * rows skip re-rendering.
   */
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
    data,
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

  const [visibleBand, setVisibleBand] = useState<VisibleBand>(() =>
    initialBand(data.length, initialElementsSize, inverted)
  );

  /*
   * Drag-to-reorder. The drag runs entirely natively (finger tracking, edge auto-
   * scroll and the make-room shuffle are all UI-thread transforms), so the React tree
   * is NOT mutated while dragging - that is what keeps it flicker-free. JS only does
   * two things: force-mount the picked-up row (draggingIndex) so it survives
   * virtualization while it is carried off-screen, and apply the single reorder on
   * drop (onDragEnd). No per-move re-render happens in between.
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
   * The flat index of the section header for the section currently at the top of the
   * viewport (the greatest sticky index at/above the topmost visible row). Drives the
   * content of the always-mounted sticky-header overlay (see renderStickyHeaderOverlay);
   * -1 when scrolled above the first section header. Updated from the visible window
   * rather than force-mounting the header element, so the pinned overlay never waits
   * on a re-render to appear.
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
      // Inverted lists report start > end; normalise to an ascending window.
      const windowLow = Math.min(visibleStartIndex, visibleEndIndex);
      const windowHigh = Math.max(visibleStartIndex, visibleEndIndex);

      setVisibleBand((prevBand) => {
        // Window already inside the band - rows are mounted, skip the re-render.
        if (
          prevBand.low >= 0 &&
          windowLow >= prevBand.low &&
          windowHigh <= prevBand.high
        ) {
          return prevBand;
        }
        const low = Math.max(0, windowLow - SHADOWLIST_OVERSCAN);
        const high = Math.min(
          data.length - 1,
          windowHigh + SHADOWLIST_OVERSCAN
        );
        slLog(
          'js.onVisibleIndicesChange apply',
          `window=[${windowLow}..${windowHigh}]`,
          `band=[${low}..${high}]`
        );
        return { low, high };
      });
    },
    [data.length]
  );

  const visibleRange = useMemo(() => bandToRange(visibleBand), [visibleBand]);

  /*
   * The dragged row must stay mounted while it is carried past the edge of the
   * window (auto-scroll), so union its index into the rendered set even if the band
   * has moved off it.
   */
  const renderIndices = useMemo(() => {
    if (
      draggingIndex < 0 ||
      draggingIndex >= data.length ||
      visibleRange.includes(draggingIndex)
    ) {
      return visibleRange;
    }
    return [...visibleRange, draggingIndex].sort((a, b) => a - b);
  }, [visibleRange, draggingIndex, data.length]);

  const elementsAllKeys = useMemo(
    () => data.map((element, index) => keyExtractor(element, index)),
    [data, keyExtractor]
  );

  /*
   * Pickup: just keep the picked-up row mounted (it may be carried off-screen by
   * auto-scroll). The data order is unchanged - the native side opens the gap by
   * translating sibling views, so nothing re-renders during the drag.
   */
  const handleDragStart: CodegenTypes.DirectEventHandler<OnDragStart, never> =
    useCallback((event) => {
      const { index } = event.nativeEvent;
      setDraggingIndex(index);
      slLog('js.onDragStart', `index=${index}`);
    }, []);

  /*
   * Drop: the single reorder. Native reports the picked-up index and its final slot;
   * apply one array move and hand the result to onReorder, then release the mount.
   */
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

  /*
   * Drag-to-reorder is gated by dragEnabled (onDragStart/onDragEnd only act while it
   * is on). If the consumer disables dragging mid-gesture (e.g. leaving edit mode),
   * the native end event is cancelled/dropped, so draggingIndex would stay latched
   * and keep the picked-up row force-mounted. Releasing it here recovers cleanly.
   */
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
      // The core reports the higher index first for inverted lists; normalise to
      // an ascending range before mapping back to items.
      const lo = Math.min(viewableStartIndex, viewableEndIndex);
      const hi = Math.max(viewableStartIndex, viewableEndIndex);

      /*
       * Drive the sticky-overlay content from the strictly-viewable top index (the
       * element actually at the viewport top), NOT the buffered visible window start
       * (which sits ~one window above the viewport): the native pin positions the
       * overlay from the raw scroll offset, so deriving the content from the buffered
       * window made it lag a whole section behind the pin. The viewable top matches
       * the pinned header to within a row.
       */
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
   * Sticky section-header overlay (SectionList). When sticky section headers are in
   * play we always mount one overlay template and only swap its content to the
   * active section's header; the native layer pins it to the viewport on the UI
   * thread, so its position never lags. Content is null while scrolled above the
   * first header (native hides the empty overlay).
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
      startReachedThreshold={onStartReachedThreshold}
      endReachedThreshold={onEndReachedThreshold}
      viewablePercentThreshold={viewablePercentThreshold}
      dragEnabled={dragEnabled}
      onStartReached={onStartReached}
      onEndReached={onEndReached}
      onScroll={onScroll}
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

/*
 * forwardRef + generics: cast preserves the generic element type for callers.
 */
const Shadowlist = forwardRef(ShadowlistInner) as <
  ElementT extends { id: string },
>(
  props: ShadowlistProps<ElementT> & { ref?: Ref<ShadowlistCommands> }
) => ReactElement;

export default Shadowlist;
