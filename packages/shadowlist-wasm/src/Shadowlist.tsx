import {
  forwardRef,
  memo,
  useCallback,
  useEffect,
  useImperativeHandle,
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
  type CSSProperties,
  type ReactElement,
  type ReactNode,
} from 'react';
import { createShadowlistCore, type ShadowlistCoreInstance } from './core.js';
import type { ShadowlistCommands, ShadowlistProps, ViewToken } from './types.js';

const DEFAULT_ESTIMATED_SIZE = 120;

/*
 * Stable default key derivation (element.id), kept module-level so the memo
 * dependencies stay referentially stable when no keyExtractor is supplied.
 */
const defaultKeyExtractor = (item: { id: string }) => item.id;

/*
 * Cap on the number of follow-up measurement passes triggered by a single
 * burst, so a layout that never settles (e.g. content that resizes itself every
 * frame) cannot spin forever. Reset whenever the user actually scrolls.
 */
const MAX_SETTLE_PASSES = 8;

/*
 * Sentinel for the scrollToIndex command channel meaning "scroll to the very end".
 * scrollToEnd reuses the same nonce-based command as scrollToIndex, and the core
 * (shadowlist-core/Constants.hpp SCROLL_TO_END_INDEX) resolves it to a correction
 * that re-targets the bottom as off-screen rows are measured, so it lands on the
 * true end of a variable-height list rather than a stale, estimate-based bottom.
 */
const SCROLL_TO_END_INDEX = -3;

/*
 * Drag-to-reorder tuning. A press must settle for LONG_PRESS_MS (without moving past
 * DRAG_SLOP, which means the user is scrolling) before a row is picked up. While
 * dragging, the pointer within DRAG_EDGE of a viewport edge auto-scrolls at up to
 * DRAG_SPEED px/frame.
 */
const LONG_PRESS_MS = 250;
const DRAG_SLOP = 8;
const DRAG_EDGE = 80;
const DRAG_SPEED = 14;

/*
 * Move the item at `from` to `to`, returning a new array. Used once on drop to
 * produce the reordered array handed to onReorder.
 */
function arrayMove<T>(input: ReadonlyArray<T>, from: number, to: number): T[] {
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
  const [moved] = next.splice(from, 1);
  next.splice(to, 0, moved as T);
  return next;
}

interface DragState {
  originIndex: number;
  insertionIndex: number;
  draggedExtent: number;
  grabOffset: number;
  desiredLeading: number;
}

/*
 * Main-axis offset for an element during a drag: the picked-up row follows the
 * pointer (desiredLeading); the siblings between the pickup and the insertion point
 * shift by one row toward the vacated pickup slot to open the gap; everything else
 * keeps its base offset. The shift equals the picked-up row's extent, so each shuffled
 * sibling lands exactly on its post-reorder resting position.
 */
function dragAdjustedOffset(
  index: number,
  mainBase: number,
  drag: DragState
): number {
  if (index === drag.originIndex) return drag.desiredLeading;
  if (
    drag.originIndex < drag.insertionIndex &&
    index > drag.originIndex &&
    index <= drag.insertionIndex
  ) {
    return mainBase - drag.draggedExtent;
  }
  if (
    drag.insertionIndex < drag.originIndex &&
    index >= drag.insertionIndex &&
    index < drag.originIndex
  ) {
    return mainBase + drag.draggedExtent;
  }
  return mainBase;
}

/*
 * Pin the always-mounted section-header overlay to the viewport start: it tracks the
 * scroll offset, pushed up as the next in-flow section header arrives. The active
 * header is the last one resting at/above the scroll offset. Hidden when scrolled
 * above the first header. Mirrors the native sticky section-header pin.
 */
function pinSectionOverlay(
  core: ShadowlistCoreInstance,
  overlay: HTMLDivElement | null,
  rawOffset: number,
  isHorizontal: boolean,
  stickyIndices: ReadonlyArray<number>
): void {
  if (!overlay) return;
  if (stickyIndices.length === 0) {
    overlay.style.display = 'none';
    return;
  }
  const scrollOffset = Math.max(0, rawOffset);
  const elementsSize = core.getElementsSize();

  let hasActive = false;
  let activeSize = 0;
  let hasNext = false;
  let nextOffset = 0;
  for (let i = 0; i < stickyIndices.length; i++) {
    const idx = stickyIndices[i];
    if (idx >= elementsSize) continue;
    const layout = core.getElementAtIndex(idx);
    const headerOffset = isHorizontal ? layout.offsetX : layout.offsetY;
    const headerSize = isHorizontal ? layout.width : layout.height;
    if (headerOffset <= scrollOffset) {
      hasActive = true;
      activeSize = headerSize;
    } else {
      nextOffset = headerOffset;
      hasNext = true;
      break;
    }
  }

  if (!hasActive) {
    overlay.style.display = 'none';
    return;
  }

  let translation = scrollOffset;
  if (hasNext) {
    const pushedTop = nextOffset - activeSize;
    if (pushedTop < translation) translation = pushedTop;
  }

  overlay.style.display = '';
  overlay.style.transform = isHorizontal
    ? `translate(${translation}px, 0px)`
    : `translate(0px, ${translation}px)`;
}

function resolveComponent(
  component: ShadowlistProps<{ id: string }>['ListHeaderComponent']
): ReactNode {
  if (!component) return null;
  return typeof component === 'function' ? component() : component;
}

/*
 * Normalize the core's visible index range (inverted lists report start > end)
 * into the ascending, clamped list of indices to mount.
 */
function rangeFromVisible(
  visibleStartIndex: number,
  visibleEndIndex: number,
  dataLength: number
): number[] {
  if (dataLength === 0 || visibleStartIndex < 0 || visibleEndIndex < 0) {
    return [];
  }
  let start = Math.min(visibleStartIndex, visibleEndIndex);
  let end = Math.max(visibleStartIndex, visibleEndIndex);
  start = Math.max(0, start);
  end = Math.min(dataLength - 1, end);

  const indices: number[] = [];
  for (let index = start; index <= end; index++) {
    indices.push(index);
  }
  return indices;
}

function initialRange(
  dataLength: number,
  initialElementsSize: number,
  inverted: boolean
): number[] {
  if (dataLength === 0) return [];
  if (inverted) {
    return rangeFromVisible(
      dataLength - initialElementsSize,
      dataLength - 1,
      dataLength
    );
  }
  return rangeFromVisible(0, initialElementsSize, dataLength);
}

const SHADOWLIST_OVERSCAN = 4;

/*
 * Mounted band (low/high-water mark). The core reports a buffered visible window;
 * bandRange mounts SHADOWLIST_OVERSCAN extra rows on each side and we only
 * re-render to grow/shift the band when the window leaves it (rangeCovers is
 * false). Windows are normalised to ascending, so inverted lists use the same path.
 */
function rangeCovers(mounted: number[], vs: number, ve: number): boolean {
  if (mounted.length === 0) return false;
  return vs >= mounted[0] && ve <= mounted[mounted.length - 1];
}

function bandRange(vs: number, ve: number, dataLength: number): number[] {
  if (dataLength === 0 || vs < 0 || ve < 0) return [];
  const low = Math.max(0, vs - SHADOWLIST_OVERSCAN);
  const high = Math.min(dataLength - 1, ve + SHADOWLIST_OVERSCAN);
  const indices: number[] = [];
  for (let index = low; index <= high; index++) indices.push(index);
  return indices;
}

interface ElementRendererProps<ElementT> {
  index: number;
  element: ElementT;
  renderElement: (info: { element: ElementT; index: number }) => ReactNode;
  separator: ReactNode;
  registerRef: (index: number, node: HTMLDivElement | null) => void;
  elementStyle?: CSSProperties;
}

/*
 * Absolutely positioned wrapper. Its transform/size are applied imperatively
 * from the core after each measurement pass (not via React) to avoid a render
 * just to reposition.
 */
const ElementRenderer = memo(function ElementRenderer<
  ElementT extends { id: string },
>({
  index,
  element,
  renderElement,
  separator,
  registerRef,
  elementStyle,
}: ElementRendererProps<ElementT>) {
  const ref = useCallback(
    (node: HTMLDivElement | null) => registerRef(index, node),
    [index, registerRef]
  );
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
    <div
      ref={ref}
      data-shadowlist-index={index}
      style={{ position: 'absolute', top: 0, left: 0, ...elementStyle }}
    >
      {children}
    </div>
  );
}) as <T extends { id: string }>(
  props: ElementRendererProps<T>
) => ReactElement;

function ShadowlistInner<ElementT extends { id: string }>(
  props: ShadowlistProps<ElementT>,
  ref: React.Ref<ShadowlistCommands>
) {
  const {
    data,
    renderElement,
    keyExtractor = defaultKeyExtractor,
    style,
    elementStyle,
    inverted = false,
    horizontal = false,
    stickyHeader = false,
    stickyFooter = false,
    columns = 1,
    stickyHeaderIndices,
    renderStickyHeaderOverlay,
    dragEnabled = false,
    onReorder,
    containerOffsetIndex = -2,
    initialElementsSize = 20,
    estimatedElementWidth = DEFAULT_ESTIMATED_SIZE,
    estimatedElementHeight = DEFAULT_ESTIMATED_SIZE,
    onStartReached,
    onEndReached,
    onStartReachedThreshold = 1,
    onEndReachedThreshold = 1,
    viewabilityConfig,
    onViewableItemsChanged,
    ItemSeparatorComponent,
    ListHeaderComponent,
    ListFooterComponent,
    ListEmptyComponent,
  } = props;

  const viewablePercentThreshold =
    (viewabilityConfig?.itemVisiblePercentThreshold ?? 0) / 100;

  const scrollRef = useRef<HTMLDivElement | null>(null);
  const contentRef = useRef<HTMLDivElement | null>(null);
  const headerRef = useRef<HTMLDivElement | null>(null);
  const footerRef = useRef<HTMLDivElement | null>(null);
  const sectionOverlayRef = useRef<HTMLDivElement | null>(null);
  const elementRefs = useRef<Map<number, HTMLDivElement>>(new Map());

  /*
   * Sticky section headers (SectionList). The flat indices of the section-header rows;
   * the topmost one resting at/above the scroll offset is pinned in an always-mounted
   * overlay (its content is renderStickyHeaderOverlay(activeStickyIndex)), pushed up as
   * the next header arrives. activeStickyIndex drives the overlay content; the pin
   * position is applied imperatively every scroll frame.
   */
  const stickyIndicesRef = useRef<ReadonlyArray<number>>(stickyHeaderIndices ?? []);
  stickyIndicesRef.current = stickyHeaderIndices ?? [];
  const [activeStickyIndex, setActiveStickyIndex] = useState(-1);

  /*
   * Drag-to-reorder. The data order is FIXED during the drag - the picked-up row
   * follows the pointer and the siblings between it and the insertion point are
   * shuffled, all via imperative transforms in applyPositions (no re-render). The
   * single reorder is applied on drop. draggingIndex force-mounts the picked-up row so
   * it survives virtualization while auto-scroll carries it off-screen.
   */
  const dragStateRef = useRef<{
    originIndex: number;
    insertionIndex: number;
    draggedExtent: number;
    grabOffset: number;
    desiredLeading: number;
  } | null>(null);
  const [draggingIndex, setDraggingIndex] = useState(-1);
  const dragRafRef = useRef<number | null>(null);
  const dragPointerClientRef = useRef(0);
  const dragPointerIdRef = useRef<number | null>(null);
  const dragCandidateRef = useRef<{
    index: number;
    client: number;
    pointerId: number;
    timer: number;
  } | null>(null);

  const coreRef = useRef<ShadowlistCoreInstance | null>(null);
  const [coreReady, setCoreReady] = useState(false);

  /*
   * What the DOM currently holds: its scroll offset and the last published
   * content size. Fed into resolveStateUpdate so the core only moves the view
   * when it actually wants to (and never fights the user's scrolling).
   */
  const publishedRef = useRef({
    containerOffsetX: 0,
    containerOffsetY: 0,
    totalContainerWidth: 0,
    totalContainerHeight: 0,
  });

  /*
   * scrollToIndex is an imperative command; bump the nonce on every call so the
   * core re-scrolls even when targeting the same index twice.
   */
  const commandRef = useRef({ index: -1, nonce: 0 });

  const rafRef = useRef<number | null>(null);
  const settlePassesRef = useRef(0);
  const ignoreScrollRef = useRef(false);
  // Set on a genuine user scroll, consumed by the next tick. Tells the core to
  // drop any in-flight scroll correction so the user is not snapped back to it.
  const userScrolledRef = useRef(false);
  const mountedRangeRef = useRef<number[]>([]);
  // Previously-viewable tokens, used to compute the `changed` set for onViewableItemsChanged.
  const prevViewableRef = useRef<ViewToken<ElementT>[]>([]);

  const [range, setRange] = useState<number[]>(() =>
    initialRange(data.length, initialElementsSize, inverted)
  );
  mountedRangeRef.current = range;

  // Latest values for use inside stable callbacks.
  const latestRef = useRef({
    data,
    horizontal,
    inverted,
    columns,
    estimatedElementWidth,
    estimatedElementHeight,
    containerOffsetIndex,
    stickyHeader,
    stickyFooter,
    dragEnabled,
    startReachedThreshold: onStartReachedThreshold,
    endReachedThreshold: onEndReachedThreshold,
    viewablePercentThreshold,
    keyExtractor,
  });
  latestRef.current = {
    data,
    horizontal,
    inverted,
    columns,
    estimatedElementWidth,
    estimatedElementHeight,
    containerOffsetIndex,
    stickyHeader,
    stickyFooter,
    dragEnabled,
    startReachedThreshold: onStartReachedThreshold,
    endReachedThreshold: onEndReachedThreshold,
    viewablePercentThreshold,
    keyExtractor,
  };

  const keys = useMemo(
    () => data.map((element, index) => keyExtractor(element, index)),
    [data, keyExtractor]
  );
  const keysRef = useRef(keys);
  keysRef.current = keys;

  const registerRef = useCallback((index: number, node: HTMLDivElement | null) => {
    if (node) {
      elementRefs.current.set(index, node);
    } else {
      elementRefs.current.delete(index);
    }
  }, []);

  /*
   * Position the mounted DOM nodes (elements, header, footer) from the layout
   * the core computed, and size the content box.
   */
  const applyPositions = useCallback(() => {
    const core = coreRef.current;
    const content = contentRef.current;
    if (!core || !content) return;

    const { horizontal: isHorizontal, columns: columnCount } = latestRef.current;
    const elementsSize = core.getElementsSize();
    const drag = dragStateRef.current;

    elementRefs.current.forEach((node, index) => {
      if (index >= elementsSize) return;
      const layout = core.getElementAtIndex(index);
      let tx: number;
      let ty: number;
      if (columnCount > 1) {
        tx = layout.offsetX;
        ty = layout.offsetY;
        node.style.width = `${layout.width}px`;
        node.style.height = '';
      } else if (isHorizontal) {
        tx = layout.offsetX;
        ty = 0;
        node.style.height = '100%';
        node.style.width = '';
      } else {
        tx = 0;
        ty = layout.offsetY;
        node.style.width = '100%';
        node.style.height = '';
      }

      if (drag) {
        // Follow the pointer / open the make-room gap along the main axis.
        if (isHorizontal) {
          tx = dragAdjustedOffset(index, tx, drag);
        } else {
          ty = dragAdjustedOffset(index, ty, drag);
        }
        // Lift the picked-up row above its siblings.
        if (index === drag.originIndex) {
          node.style.zIndex = '3';
          node.style.boxShadow = '0 6px 16px rgba(0, 0, 0, 0.25)';
        } else if (node.style.zIndex || node.style.boxShadow) {
          node.style.zIndex = '';
          node.style.boxShadow = '';
        }
      } else if (node.style.zIndex || node.style.boxShadow) {
        node.style.zIndex = '';
        node.style.boxShadow = '';
      }

      node.style.transform = `translate(${tx}px, ${ty}px)`;
    });

    // Pin the section-header overlay from the same layout (its content is driven by
    // activeStickyIndex; this only moves it).
    const scrollEl = scrollRef.current;
    if (scrollEl && sectionOverlayRef.current) {
      pinSectionOverlay(
        core,
        sectionOverlayRef.current,
        isHorizontal ? scrollEl.scrollLeft : scrollEl.scrollTop,
        isHorizontal,
        stickyIndicesRef.current
      );
    }

    if (headerRef.current) {
      // Sticky header tracks the scroll offset; getStickyHeaderOffset returns 0
      // (the resting position) when stickyHeader is off.
      const headerOffset = core.getStickyHeaderOffset();
      headerRef.current.style.transform = isHorizontal
        ? `translate(${headerOffset}px, 0px)`
        : `translate(0px, ${headerOffset}px)`;
      if (!isHorizontal) headerRef.current.style.width = '100%';
    }

    if (footerRef.current) {
      const footerSize = isHorizontal
        ? footerRef.current.offsetWidth
        : footerRef.current.offsetHeight;
      // Sticky footer tracks the viewport end; getStickyFooterOffset returns the
      // resting offset (totalSize - footerSize) when stickyFooter is off.
      const footerOffset = core.getStickyFooterOffset(footerSize);
      footerRef.current.style.transform = isHorizontal
        ? `translate(${footerOffset}px, 0px)`
        : `translate(0px, ${footerOffset}px)`;
      if (!isHorizontal) footerRef.current.style.width = '100%';
    }
  }, []);

  const scheduleTick = useCallback(() => {
    if (rafRef.current != null) return;
    rafRef.current = requestAnimationFrame(() => {
      rafRef.current = null;
      tickRef.current();
    });
  }, []);

  /*
   * The active section header is the greatest sticky index at/above the topmost
   * visible row; it drives the overlay's content. -1 when scrolled above the first.
   */
  const updateActiveStickyIndex = useCallback((windowLow: number) => {
    const indices = stickyIndicesRef.current;
    if (indices.length === 0) {
      setActiveStickyIndex((prev) => (prev === -1 ? prev : -1));
      return;
    }
    let active = -1;
    for (const idx of indices) {
      if (idx <= windowLow) active = idx;
      else break;
    }
    setActiveStickyIndex((prev) => (prev === active ? prev : active));
  }, []);

  /*
   * One virtualization frame: read scroll/window/header/footer geometry, run the
   * core (reconcile + measure + resolve scroll), then decide which window to
   * mount. If the window changed React re-renders and the layout effect runs the
   * measurement pass; otherwise we run it directly.
   */
  const tick = useCallback(() => {
    const core = coreRef.current;
    const scroll = scrollRef.current;
    if (!core || !scroll) return;

    const {
      horizontal: isHorizontal,
      inverted: isInverted,
      columns: columnCount,
      estimatedElementWidth: estimatedWidth,
      estimatedElementHeight: estimatedHeight,
      containerOffsetIndex: propIndex,
      data: currentData,
      stickyHeader: isStickyHeader,
      stickyFooter: isStickyFooter,
      startReachedThreshold: startThreshold,
      endReachedThreshold: endThreshold,
      viewablePercentThreshold: viewableThreshold,
    } = latestRef.current;

    const containerOffsetX = scroll.scrollLeft;
    const containerOffsetY = scroll.scrollTop;
    const windowContainerWidth = scroll.clientWidth;
    const windowContainerHeight = scroll.clientHeight;

    const headerSize = headerRef.current
      ? isHorizontal
        ? headerRef.current.offsetWidth
        : headerRef.current.offsetHeight
      : 0;
    const footerSize = footerRef.current
      ? isHorizontal
        ? footerRef.current.offsetWidth
        : footerRef.current.offsetHeight
      : 0;

    core.requestScrollToIndex(
      commandRef.current.index,
      commandRef.current.nonce,
      propIndex
    );

    // Consume the user-scroll flag: only the tick this gesture scheduled is
    // user-initiated; the settle passes that follow are corrections, not gestures.
    const userScrolled = userScrolledRef.current;
    userScrolledRef.current = false;

    core.update(
      keysRef.current,
      containerOffsetX,
      containerOffsetY,
      windowContainerWidth,
      windowContainerHeight,
      headerSize,
      footerSize,
      isInverted,
      isHorizontal,
      columnCount,
      estimatedWidth,
      estimatedHeight,
      userScrolled,
      isStickyHeader,
      isStickyFooter,
      startThreshold,
      endThreshold,
      viewableThreshold
    );

    const visible = core.getVisibleIndices();
    const windowLow = Math.min(visible.visibleStartIndex, visible.visibleEndIndex);
    const windowHigh = Math.max(visible.visibleStartIndex, visible.visibleEndIndex);
    const validWindow =
      visible.visibleStartIndex >= 0 && visible.visibleEndIndex >= 0;

    if (validWindow) updateActiveStickyIndex(windowLow);

    // Low/high-water: only grow/shift the mounted band when the reported window
    // leaves it; while it stays inside, the rows are mounted so skip the re-render.
    if (validWindow && !rangeCovers(mountedRangeRef.current, windowLow, windowHigh)) {
      const nextRange = bandRange(windowLow, windowHigh, currentData.length);
      mountedRangeRef.current = nextRange;
      setRange(nextRange);
      // The layout effect will run measureAndResolve after the commit.
      return;
    }

    measureAndResolveRef.current();
  }, [updateActiveStickyIndex]);

  /*
   * Feed measured DOM sizes back into the core, refresh the content size, apply
   * positions, then publish the resolved state (content size + scroll
   * correction). Schedules another pass while things are still moving.
   */
  const measureAndResolve = useCallback(() => {
    const core = coreRef.current;
    const scroll = scrollRef.current;
    const content = contentRef.current;
    if (!core || !scroll || !content) return;

    const { horizontal: isHorizontal } = latestRef.current;
    const elementsSize = core.getElementsSize();

    /*
     * Constrain each node to the cross-axis size the core controls (track width
     * for columns, 100% for single column / horizontal) BEFORE measuring, so the
     * measured main-axis size reflects that constraint.
     */
    applyPositions();

    elementRefs.current.forEach((node, index) => {
      if (index >= elementsSize) return;
      core.updateElementAtIndex(index, node.offsetWidth, node.offsetHeight);
    });
    core.recomputeTotalSize();

    applyPositions();

    const published = publishedRef.current;
    const stateUpdate = core.resolveStateUpdate(
      published.containerOffsetX,
      published.containerOffsetY,
      published.totalContainerWidth,
      published.totalContainerHeight
    );

    let needsAnotherPass = false;

    if (stateUpdate.changed) {
      published.totalContainerWidth = stateUpdate.totalContainerWidth;
      published.totalContainerHeight = stateUpdate.totalContainerHeight;

      if (isHorizontal) {
        content.style.width = `${stateUpdate.totalContainerWidth}px`;
        content.style.height = '100%';
      } else {
        content.style.height = `${stateUpdate.totalContainerHeight}px`;
        content.style.width = '100%';
      }

      // While dragging, the drag owns the scroll position (the auto-scroll loop drives
      // it), so a core offset correction must not yank the content under the pointer.
      if (stateUpdate.applyContainerOffset && !dragStateRef.current) {
        ignoreScrollRef.current = true;
        scroll.scrollLeft = stateUpdate.containerOffsetX;
        scroll.scrollTop = stateUpdate.containerOffsetY;
        published.containerOffsetX = stateUpdate.containerOffsetX;
        published.containerOffsetY = stateUpdate.containerOffsetY;
        needsAnotherPass = true;
      }
    }

    published.containerOffsetX = scroll.scrollLeft;
    published.containerOffsetY = scroll.scrollTop;

    // The corrected offset / freshly measured sizes may shift the visible window
    // out of the mounted band, which needs another pass to mount the new rows.
    const visible = core.getVisibleIndices();
    const windowLow = Math.min(visible.visibleStartIndex, visible.visibleEndIndex);
    const windowHigh = Math.max(visible.visibleStartIndex, visible.visibleEndIndex);
    if (
      visible.visibleStartIndex >= 0 &&
      visible.visibleEndIndex >= 0 &&
      !rangeCovers(mountedRangeRef.current, windowLow, windowHigh)
    ) {
      needsAnotherPass = true;
    }

    if (needsAnotherPass && settlePassesRef.current < MAX_SETTLE_PASSES) {
      settlePassesRef.current += 1;
      scheduleTick();
    }
  }, [applyPositions, scheduleTick]);

  // Stable indirection so tick/measureAndResolve can reference each other.
  const tickRef = useRef(tick);
  tickRef.current = tick;
  const measureAndResolveRef = useRef(measureAndResolve);
  measureAndResolveRef.current = measureAndResolve;

  // Create the core and wire callbacks once.
  useEffect(() => {
    let disposed = false;
    let instance: ShadowlistCoreInstance | null = null;

    createShadowlistCore().then((created) => {
      if (disposed) {
        created.delete();
        return;
      }
      instance = created;
      coreRef.current = created;

      created.setOnScroll((containerOffsetX, containerOffsetY) => {
        latestOnScroll.current?.({
          nativeEvent: {
            contentOffsetX: containerOffsetX,
            contentOffsetY: containerOffsetY,
          },
        });
      });
      created.setOnStartReached(() => latestOnStartReached.current?.());
      created.setOnEndReached(() => latestOnEndReached.current?.());
      created.setOnViewableIndicesChange((startIndex, endIndex) =>
        dispatchViewable(startIndex, endIndex)
      );

      setCoreReady(true);
      scheduleTick();
    });

    return () => {
      disposed = true;
      if (rafRef.current != null) cancelAnimationFrame(rafRef.current);
      coreRef.current = null;
      instance?.delete();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Keep the callback refs fresh without re-wiring the core.
  const latestOnStartReached = useRef(onStartReached);
  latestOnStartReached.current = onStartReached;
  const latestOnEndReached = useRef(onEndReached);
  latestOnEndReached.current = onEndReached;
  const latestOnScroll = useRef(props.onScroll);
  latestOnScroll.current = props.onScroll;
  const latestOnViewableItemsChanged = useRef(onViewableItemsChanged);
  latestOnViewableItemsChanged.current = onViewableItemsChanged;
  const latestOnReorder = useRef(onReorder);
  latestOnReorder.current = onReorder;

  /*
   * Map the core's viewable index range to FlatList-style viewable/changed tokens.
   * Reads from refs so it stays stable and can be wired into the core once.
   */
  const dispatchViewable = useCallback((startIndex: number, endIndex: number) => {
    const onChanged = latestOnViewableItemsChanged.current;
    if (!onChanged) return;

    const { data: currentData, keyExtractor: keyOf } = latestRef.current;

    const viewableItems: ViewToken<ElementT>[] = [];
    if (startIndex >= 0 && endIndex >= 0) {
      const lo = Math.min(startIndex, endIndex);
      const hi = Math.max(startIndex, endIndex);
      for (let index = lo; index <= hi; index++) {
        const item = currentData[index];
        if (!item) continue;
        viewableItems.push({ item, index, key: keyOf(item, index), isViewable: true });
      }
    }

    const currentKeys = new Set(viewableItems.map((token) => token.key));
    const prevKeys = new Set(prevViewableRef.current.map((token) => token.key));
    const changed: ViewToken<ElementT>[] = [
      ...viewableItems.filter((token) => !prevKeys.has(token.key)),
      ...prevViewableRef.current
        .filter((token) => !currentKeys.has(token.key))
        .map((token) => ({ ...token, isViewable: false })),
    ];

    prevViewableRef.current = viewableItems;
    if (changed.length > 0) {
      onChanged({ viewableItems, changed });
    }
  }, []);

  // Re-run the core whenever the data set or layout configuration changes.
  useEffect(() => {
    if (!coreReady) return;
    settlePassesRef.current = 0;
    scheduleTick();
  }, [
    coreReady,
    keys,
    inverted,
    horizontal,
    columns,
    estimatedElementWidth,
    estimatedElementHeight,
    containerOffsetIndex,
    scheduleTick,
  ]);

  // After every render that changes the mounted window, run a measurement pass.
  useLayoutEffect(() => {
    if (!coreReady) return;
    measureAndResolveRef.current();
  }, [coreReady, range]);

  /*
   * Pin the sticky header/footer straight from live scroll geometry, synchronously
   * in the scroll event. applyPositions also pins them, but only on the rAF tick
   * after the core runs - too late to track the finger, which reads as lag. This
   * computes the same content-space position the core would (offset for the header,
   * offset + window - footerSize for the footer) without waiting for the pipeline.
   */
  const pinStickyEdges = useCallback(() => {
    const scroll = scrollRef.current;
    if (!scroll) return;
    const {
      horizontal: isHorizontal,
      stickyHeader: isStickyHeader,
      stickyFooter: isStickyFooter,
    } = latestRef.current;

    // Pin the section-header overlay straight from live scroll geometry too, so the
    // pinned section header never lags the scroll.
    const core = coreRef.current;
    if (core && sectionOverlayRef.current) {
      pinSectionOverlay(
        core,
        sectionOverlayRef.current,
        isHorizontal ? scroll.scrollLeft : scroll.scrollTop,
        isHorizontal,
        stickyIndicesRef.current
      );
    }

    if (!isStickyHeader && !isStickyFooter) return;

    const offset = isHorizontal ? scroll.scrollLeft : scroll.scrollTop;
    const windowSize = isHorizontal ? scroll.clientWidth : scroll.clientHeight;

    if (isStickyHeader && headerRef.current) {
      headerRef.current.style.transform = isHorizontal
        ? `translate(${offset}px, 0px)`
        : `translate(0px, ${offset}px)`;
    }
    if (isStickyFooter && footerRef.current) {
      const footerSize = isHorizontal
        ? footerRef.current.offsetWidth
        : footerRef.current.offsetHeight;
      const footerOffset = offset + windowSize - footerSize;
      footerRef.current.style.transform = isHorizontal
        ? `translate(${footerOffset}px, 0px)`
        : `translate(0px, ${footerOffset}px)`;
    }
  }, []);

  // Native scroll -> new frame. Ignore the scroll we triggered ourselves.
  const handleScroll = useCallback(() => {
    // Keep the pinned edges glued to the viewport on the scroll event itself,
    // before the (heavier, rAF-deferred) virtualization pass runs.
    pinStickyEdges();

    if (ignoreScrollRef.current) {
      ignoreScrollRef.current = false;
      return;
    }
    settlePassesRef.current = 0;
    userScrolledRef.current = true;
    scheduleTick();
  }, [pinStickyEdges, scheduleTick]);

  /*
   * Drag-to-reorder. The data order is fixed during the drag; updateDrag glues the
   * picked-up row to the pointer, recomputes where it would insert (midpoints over the
   * fixed base offsets, so no oscillation), and applyPositions shuffles the siblings -
   * all imperative, no re-render. The single reorder is applied on drop.
   */
  const updateDrag = useCallback(() => {
    const drag = dragStateRef.current;
    const core = coreRef.current;
    const scroll = scrollRef.current;
    if (!drag || !core || !scroll) return;
    const { horizontal: isHorizontal } = latestRef.current;

    const rect = scroll.getBoundingClientRect();
    const rectStart = isHorizontal ? rect.left : rect.top;
    const offset = isHorizontal ? scroll.scrollLeft : scroll.scrollTop;
    const pointerContent = dragPointerClientRef.current - rectStart + offset;
    const contentExtent = isHorizontal ? scroll.scrollWidth : scroll.scrollHeight;

    let desiredLeading = pointerContent - drag.grabOffset;
    desiredLeading = Math.max(
      0,
      Math.min(desiredLeading, Math.max(0, contentExtent - drag.draggedExtent))
    );
    drag.desiredLeading = desiredLeading;

    const center = desiredLeading + drag.draggedExtent / 2;
    let insertion = drag.originIndex;
    elementRefs.current.forEach((_node, index) => {
      if (index === drag.originIndex || index >= core.getElementsSize()) return;
      const layout = core.getElementAtIndex(index);
      const lead = isHorizontal ? layout.offsetX : layout.offsetY;
      const extent = isHorizontal ? layout.width : layout.height;
      const mid = lead + extent / 2;
      if (index > drag.originIndex && center > mid) {
        insertion = Math.max(insertion, index);
      } else if (index < drag.originIndex && center < mid) {
        insertion = Math.min(insertion, index);
      }
    });
    drag.insertionIndex = insertion;

    applyPositions();
  }, [applyPositions]);

  // Per-frame loop while dragging: auto-scroll near the edges, then reposition.
  const dragLoop = useCallback(() => {
    const drag = dragStateRef.current;
    const scroll = scrollRef.current;
    if (!drag || !scroll) {
      dragRafRef.current = null;
      return;
    }
    const { horizontal: isHorizontal } = latestRef.current;

    const rect = scroll.getBoundingClientRect();
    const rectStart = isHorizontal ? rect.left : rect.top;
    const windowSize = isHorizontal ? scroll.clientWidth : scroll.clientHeight;
    const contentSize = isHorizontal ? scroll.scrollWidth : scroll.scrollHeight;
    const maxOffset = Math.max(0, contentSize - windowSize);
    const offset = isHorizontal ? scroll.scrollLeft : scroll.scrollTop;
    const pointer = dragPointerClientRef.current - rectStart;

    let delta = 0;
    if (pointer < DRAG_EDGE) {
      delta = -DRAG_SPEED * (1 - pointer / DRAG_EDGE);
    } else if (pointer > windowSize - DRAG_EDGE) {
      delta = DRAG_SPEED * (1 - (windowSize - pointer) / DRAG_EDGE);
    }
    if (delta !== 0) {
      const newOffset = Math.min(Math.max(offset + delta, 0), maxOffset);
      if (newOffset !== offset) {
        // A real (user-owned) scroll: handleScroll flags userScrolled so the core
        // virtualizes at this exact offset instead of fighting it.
        if (isHorizontal) scroll.scrollLeft = newOffset;
        else scroll.scrollTop = newOffset;
      }
    }

    updateDrag();
    dragRafRef.current = requestAnimationFrame(dragLoop);
  }, [updateDrag]);

  const finishDrag = useCallback(() => {
    const drag = dragStateRef.current;
    const scroll = scrollRef.current;
    if (!drag) return;

    if (dragRafRef.current != null) {
      cancelAnimationFrame(dragRafRef.current);
      dragRafRef.current = null;
    }

    const from = drag.originIndex;
    const to = drag.insertionIndex;
    dragStateRef.current = null;
    setDraggingIndex(-1);

    if (scroll) {
      scroll.style.touchAction = '';
      scroll.style.userSelect = '';
      const pid = dragPointerIdRef.current;
      if (pid != null && scroll.hasPointerCapture?.(pid)) {
        try {
          scroll.releasePointerCapture(pid);
        } catch {
          // ignore - pointer already released
        }
      }
    }
    dragPointerIdRef.current = null;

    if (from !== to) {
      const currentData = latestRef.current.data as ElementT[];
      latestOnReorder.current?.({
        from,
        to,
        data: arrayMove(currentData, from, to),
      });
    }

    /*
     * Re-run positioning. Until this runs the shuffle transforms stay frozen (the
     * siblings already sit at their post-reorder positions, so it is seamless); then
     * applyPositions - now drag-free - lands everything from the reordered layout (or
     * resets to the original order if the consumer ignored onReorder).
     */
    settlePassesRef.current = 0;
    scheduleTick();
  }, [scheduleTick]);

  const beginDrag = useCallback(
    (index: number, pointerId: number) => {
      const core = coreRef.current;
      const scroll = scrollRef.current;
      if (!core || !scroll) return;
      if (index < 0 || index >= core.getElementsSize()) return;
      const { horizontal: isHorizontal } = latestRef.current;

      const layout = core.getElementAtIndex(index);
      const baseLeading = isHorizontal ? layout.offsetX : layout.offsetY;
      const extent = isHorizontal ? layout.width : layout.height;

      const rect = scroll.getBoundingClientRect();
      const rectStart = isHorizontal ? rect.left : rect.top;
      const offset = isHorizontal ? scroll.scrollLeft : scroll.scrollTop;
      const pointerContent = dragPointerClientRef.current - rectStart + offset;

      dragStateRef.current = {
        originIndex: index,
        insertionIndex: index,
        draggedExtent: extent,
        grabOffset: pointerContent - baseLeading,
        desiredLeading: baseLeading,
      };
      setDraggingIndex(index);

      // Own the gesture: stop native scroll/selection and keep receiving moves.
      scroll.style.touchAction = 'none';
      scroll.style.userSelect = 'none';
      dragPointerIdRef.current = pointerId;
      try {
        scroll.setPointerCapture(pointerId);
      } catch {
        // ignore - capture is best-effort
      }

      if (dragRafRef.current == null) {
        dragRafRef.current = requestAnimationFrame(dragLoop);
      }
      updateDrag();
    },
    [dragLoop, updateDrag]
  );

  const clearDragCandidate = useCallback(() => {
    const candidate = dragCandidateRef.current;
    if (candidate) {
      window.clearTimeout(candidate.timer);
      dragCandidateRef.current = null;
    }
  }, []);

  const handlePointerDown = useCallback(
    (event: React.PointerEvent<HTMLDivElement>) => {
      if (!latestRef.current.dragEnabled) return;
      if (event.pointerType === 'mouse' && event.button !== 0) return;
      const target = event.target as HTMLElement;
      const host = target.closest<HTMLElement>('[data-shadowlist-index]');
      if (!host) return;
      const index = Number(host.getAttribute('data-shadowlist-index'));
      if (!Number.isFinite(index)) return;

      const client = latestRef.current.horizontal
        ? event.clientX
        : event.clientY;
      dragPointerClientRef.current = client;
      const pointerId = event.pointerId;
      const timer = window.setTimeout(() => {
        dragCandidateRef.current = null;
        beginDrag(index, pointerId);
      }, LONG_PRESS_MS);
      dragCandidateRef.current = { index, client, pointerId, timer };
    },
    [beginDrag]
  );

  const handlePointerMove = useCallback(
    (event: React.PointerEvent<HTMLDivElement>) => {
      const client = latestRef.current.horizontal
        ? event.clientX
        : event.clientY;
      dragPointerClientRef.current = client;

      if (dragStateRef.current) {
        event.preventDefault();
        updateDrag();
        return;
      }
      // Moved past the slop before the long press fired: the user is scrolling.
      const candidate = dragCandidateRef.current;
      if (candidate && Math.abs(client - candidate.client) > DRAG_SLOP) {
        clearDragCandidate();
      }
    },
    [updateDrag, clearDragCandidate]
  );

  const handlePointerUp = useCallback(() => {
    clearDragCandidate();
    if (dragStateRef.current) finishDrag();
  }, [clearDragCandidate, finishDrag]);

  // Re-measure on container resize.
  useEffect(() => {
    const scroll = scrollRef.current;
    if (!scroll || typeof ResizeObserver === 'undefined') return;
    const observer = new ResizeObserver(() => {
      settlePassesRef.current = 0;
      scheduleTick();
    });
    observer.observe(scroll);
    return () => observer.disconnect();
  }, [scheduleTick]);

  useImperativeHandle(
    ref,
    () => ({
      setStartReachedEnabled: (enabled: boolean) => {
        coreRef.current?.toggleStartReached(enabled);
      },
      setEndReachedEnabled: (enabled: boolean) => {
        coreRef.current?.toggleEndReached(enabled);
      },
      scrollToIndex: (index: number) => {
        commandRef.current = {
          index,
          nonce: commandRef.current.nonce + 1,
        };
        settlePassesRef.current = 0;
        scheduleTick();
      },
      scrollToOffset: (offset: number, animated: boolean = true) => {
        const scroll = scrollRef.current;
        if (!scroll) return;
        const behavior: ScrollBehavior = animated ? 'smooth' : 'auto';
        if (latestRef.current.horizontal) {
          scroll.scrollTo({ left: offset, behavior });
        } else {
          scroll.scrollTo({ top: offset, behavior });
        }
      },
      scrollToEnd: (_animated: boolean = true) => {
        /*
         * Core-driven: ride the scrollToIndex command channel with the
         * SCROLL_TO_END_INDEX sentinel so the core converges on the true bottom as
         * off-screen rows are measured, instead of a one-shot jump to the current
         * scrollHeight (a stale estimate that stops short on a variable-height list).
         * The animated flag no longer applies (the core steps to the bottom).
         */
        commandRef.current = {
          index: SCROLL_TO_END_INDEX,
          nonce: commandRef.current.nonce + 1,
        };
        settlePassesRef.current = 0;
        scheduleTick();
      },
    }),
    [scheduleTick]
  );

  const header = useMemo(
    () => resolveComponent(ListHeaderComponent),
    [ListHeaderComponent]
  );
  const footer = useMemo(
    () => resolveComponent(ListFooterComponent),
    [ListFooterComponent]
  );
  const empty = useMemo(
    () => resolveComponent(ListEmptyComponent),
    [ListEmptyComponent]
  );
  const separator = useMemo(
    () => resolveComponent(ItemSeparatorComponent),
    [ItemSeparatorComponent]
  );

  /*
   * Sticky section-header overlay (SectionList): always mounted when sticky section
   * headers are in play; its content is the active section's header and its position
   * is pinned imperatively (pinSectionOverlay), so it never lags the scroll.
   */
  const stickyEnabled = Boolean(
    stickyHeaderIndices &&
      stickyHeaderIndices.length > 0 &&
      renderStickyHeaderOverlay
  );
  const stickyOverlayContent = useMemo(
    () =>
      renderStickyHeaderOverlay && activeStickyIndex >= 0
        ? renderStickyHeaderOverlay(activeStickyIndex)
        : null,
    [renderStickyHeaderOverlay, activeStickyIndex]
  );

  /*
   * The dragged row must stay mounted while it is carried past the edge of the window
   * (auto-scroll), so union its index into the rendered set even if the band moved off
   * it. Outside a drag this is just the band.
   */
  const renderIndices = useMemo(() => {
    if (
      draggingIndex < 0 ||
      draggingIndex >= data.length ||
      range.includes(draggingIndex)
    ) {
      return range;
    }
    return [...range, draggingIndex].sort((a, b) => a - b);
  }, [range, draggingIndex, data.length]);

  const containerStyle: CSSProperties = {
    position: 'relative',
    overflow: 'auto',
    WebkitOverflowScrolling: 'touch',
    ...style,
  };

  return (
    <div
      ref={scrollRef}
      style={containerStyle}
      onScroll={handleScroll}
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={handlePointerUp}
      onPointerCancel={handlePointerUp}
    >
      <div ref={contentRef} style={{ position: 'relative', minHeight: '100%' }}>
        {header && (
          <div
            ref={headerRef}
            style={{ position: 'absolute', top: 0, left: 0, width: '100%' }}
          >
            {header}
          </div>
        )}

        {data.length === 0 && empty ? (
          <div style={{ position: 'absolute', top: 0, left: 0, width: '100%' }}>
            {empty}
          </div>
        ) : (
          renderIndices.map((index) => {
            const element = data[index];
            if (!element) return null;
            return (
              <ElementRenderer
                key={keyExtractor(element, index)}
                index={index}
                element={element}
                renderElement={renderElement}
                separator={separator && index < data.length - 1 ? separator : null}
                registerRef={registerRef}
                elementStyle={elementStyle}
              />
            );
          })
        )}

        {footer && (
          <div
            ref={footerRef}
            style={{ position: 'absolute', top: 0, left: 0, width: '100%' }}
          >
            {footer}
          </div>
        )}

        {stickyEnabled && (
          <div
            ref={sectionOverlayRef}
            style={{
              position: 'absolute',
              top: 0,
              left: 0,
              width: '100%',
              zIndex: 2,
              display: 'none',
            }}
          >
            {stickyOverlayContent}
          </div>
        )}
      </div>
    </div>
  );
}

/*
 * forwardRef + generics: cast preserves the generic element type for callers.
 */
const Shadowlist = forwardRef(ShadowlistInner) as <ElementT extends { id: string }>(
  props: ShadowlistProps<ElementT> & { ref?: React.Ref<ShadowlistCommands> }
) => ReactElement;

export default Shadowlist;
