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
import type { ShadowlistCommands, ShadowlistProps } from './types.js';

const DEFAULT_ESTIMATED_SIZE = 120;

/*
 * Cap on the number of follow-up measurement passes triggered by a single
 * burst, so a layout that never settles (e.g. content that resizes itself every
 * frame) cannot spin forever. Reset whenever the user actually scrolls.
 */
const MAX_SETTLE_PASSES = 8;

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

function sameRange(a: number[], b: number[]): boolean {
  if (a.length !== b.length) return false;
  return a[0] === b[0] && a[a.length - 1] === b[b.length - 1];
}

interface ElementWrapperProps {
  index: number;
  registerRef: (index: number, node: HTMLDivElement | null) => void;
  elementStyle?: CSSProperties;
  children: ReactNode;
}

/*
 * Absolutely positioned wrapper. Its transform/size are applied imperatively
 * from the core after each measurement pass (not via React) to avoid a render
 * just to reposition.
 */
const ElementWrapper = memo(function ElementWrapper({
  index,
  registerRef,
  elementStyle,
  children,
}: ElementWrapperProps) {
  const ref = useCallback(
    (node: HTMLDivElement | null) => registerRef(index, node),
    [index, registerRef]
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
});

function ShadowlistInner<ElementT extends { id: string }>(
  props: ShadowlistProps<ElementT>,
  ref: React.Ref<ShadowlistCommands>
) {
  const {
    data,
    renderElement,
    style,
    elementStyle,
    inverted = false,
    horizontal = false,
    columns = 1,
    containerOffsetIndex = -1,
    initialElementsSize = 20,
    estimatedElementWidth = DEFAULT_ESTIMATED_SIZE,
    estimatedElementHeight = DEFAULT_ESTIMATED_SIZE,
    onStartReached,
    onEndReached,
    ListHeaderComponent,
    ListFooterComponent,
    ListEmptyComponent,
  } = props;

  const scrollRef = useRef<HTMLDivElement | null>(null);
  const contentRef = useRef<HTMLDivElement | null>(null);
  const headerRef = useRef<HTMLDivElement | null>(null);
  const footerRef = useRef<HTMLDivElement | null>(null);
  const elementRefs = useRef<Map<number, HTMLDivElement>>(new Map());

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
  });
  latestRef.current = {
    data,
    horizontal,
    inverted,
    columns,
    estimatedElementWidth,
    estimatedElementHeight,
    containerOffsetIndex,
  };

  const keys = useMemo(() => data.map((element) => element.id), [data]);
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

    elementRefs.current.forEach((node, index) => {
      if (index >= elementsSize) return;
      const layout = core.getElementAtIndex(index);
      if (columnCount > 1) {
        node.style.transform = `translate(${layout.offsetX}px, ${layout.offsetY}px)`;
        node.style.width = `${layout.width}px`;
        node.style.height = '';
      } else if (isHorizontal) {
        node.style.transform = `translate(${layout.offsetX}px, 0px)`;
        node.style.height = '100%';
        node.style.width = '';
      } else {
        node.style.transform = `translate(0px, ${layout.offsetY}px)`;
        node.style.width = '100%';
        node.style.height = '';
      }
    });

    if (headerRef.current) {
      headerRef.current.style.transform = 'translate(0px, 0px)';
      if (!isHorizontal) headerRef.current.style.width = '100%';
    }

    if (footerRef.current) {
      const footerSize = isHorizontal
        ? footerRef.current.offsetWidth
        : footerRef.current.offsetHeight;
      const footerOffset = core.getFooterOffset(footerSize);
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
      userScrolled
    );

    const visible = core.getVisibleIndices();
    const nextRange = rangeFromVisible(
      visible.visibleStartIndex,
      visible.visibleEndIndex,
      currentData.length
    );

    if (!sameRange(nextRange, mountedRangeRef.current)) {
      mountedRangeRef.current = nextRange;
      setRange(nextRange);
      // The layout effect will run measureAndResolve after the commit.
      return;
    }

    measureAndResolveRef.current();
  }, []);

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

    const { horizontal: isHorizontal, data: currentData } = latestRef.current;
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

      if (stateUpdate.applyContainerOffset) {
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

    // The corrected offset / freshly measured sizes may shift the visible window.
    const visible = core.getVisibleIndices();
    const nextRange = rangeFromVisible(
      visible.visibleStartIndex,
      visible.visibleEndIndex,
      currentData.length
    );
    if (!sameRange(nextRange, mountedRangeRef.current)) {
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

  // Native scroll -> new frame. Ignore the scroll we triggered ourselves.
  const handleScroll = useCallback(() => {
    if (ignoreScrollRef.current) {
      ignoreScrollRef.current = false;
      return;
    }
    settlePassesRef.current = 0;
    userScrolledRef.current = true;
    scheduleTick();
  }, [scheduleTick]);

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

  const containerStyle: CSSProperties = {
    position: 'relative',
    overflow: 'auto',
    WebkitOverflowScrolling: 'touch',
    ...style,
  };

  return (
    <div ref={scrollRef} style={containerStyle} onScroll={handleScroll}>
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
          range.map((index) => {
            const element = data[index];
            if (!element) return null;
            return (
              <ElementWrapper
                key={element.id}
                index={index}
                registerRef={registerRef}
                elementStyle={elementStyle}
              >
                {renderElement({ element, index })}
              </ElementWrapper>
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
