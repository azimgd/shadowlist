import {
  registerNativeModule,
  registerViewManager,
  type NativeView,
  type PointerEvent,
  type WheelEvent,
} from 'react-native-tty/native';
import {
  PHASE_DRAGGING,
  PHASE_IDLE,
  ShadowListKitCore,
  shadowListStoreModule,
  type PassResult,
} from './ShadowListKitCore';

/*
 * The native side of Fabric's ShadowList.tsx in a terminal: the view managers behind
 * ShadowListView, ShadowListCellView and ShadowListTemplateView, and the ShadowListStore module.
 * React renders the rows the mounted range names, and this view lays them out through the core on
 * every paint. Rows are only children of the view: it paints nothing itself.
 */

const CELL_VIEW = 'ShadowListCellView';

/*
 * What a child of the view is, past what NativeView promises. A native view that places its own
 * children, as this list does, goes through react-native-tty's node: Yoga holds where the row is,
 * so measure and pointer coordinates in it stay right after the next layout, and syncLayout moves
 * it for the paint already running.
 */
interface ChildNode {
  readonly type: string;
  readonly props: Record<string, any>;
  readonly width: number;
  readonly height: number;
  readonly left: number;
  readonly top: number;
  style: Record<string, unknown>;
  readonly yoga: {
    setPosition(edge: number, value: number): void;
  } | null;
  syncLayout(parentX: number, parentY: number, events: (() => void)[]): void;
}

interface ListNode extends NativeView {
  readonly children: ChildNode[];
  style: Record<string, unknown>;
}

/*
 * Yoga's Edge.Left and Edge.Top. Its enums belong to the platform's copy of Yoga.
 */
const EDGE_LEFT = 0;
const EDGE_TOP = 1;

const OFFSCREEN = -100000;

/*
 * Passes of the core one paint may run before it waits for the next frame.
 */
const MAX_PASSES_PER_PAINT = 4;

/*
 * How often a report held back for minimumViewTime is looked at.
 */
const VIEWABILITY_POLL_MS = 50;
const NO_EVENTS: (() => void)[] = [];

interface Placement {
  x: number;
  y: number;
}

/*
 * The children of a list by what they are.
 */
interface ListChildren {
  cells: Map<string, ChildNode>;
  header: ChildNode | null;
  footer: ChildNode | null;
  empty: ChildNode | null;
  overlay: ChildNode | null;
}

class ListViewController {
  private readonly core = new ShadowListKitCore();
  private readonly placements = new WeakMap<ChildNode, Placement>();
  private offset = 0;
  private content = 0;
  private footerStart = 0;
  private bandLow = 1;
  private bandHigh = 0;
  private windowAlong = 0;
  private windowCross = 0;
  private headerSize = 0;
  private footerSize = 0;
  private adoptedStore = 0;
  private adoptedVersion = 0;
  private needsFrame = true;
  private hasSettings = false;
  private userScrolled = false;
  private wheelMoved = false;
  private pendingScrollIndex = -1;
  private heldAnchor: { key: string; offset: number } | null = null;
  private startReachedEnabled = true;
  private endReachedEnabled = true;

  /*
   * What the last events said, so each is only sent on a change.
   */
  private sentOffset = 0;
  private sentContent = -1;
  private sentAtEnd = true;
  private sentVisibleLow = -1;
  private sentVisibleHigh = -1;
  private sentActiveSticky = -1;
  private viewabilityStale = true;
  private viewabilityTimer: ReturnType<typeof setTimeout> | null = null;

  /*
   * The newest range sent to React, and the generation it was built on.
   */
  private sentSequence = 0;
  private sentGeneration = 0;

  constructor(private readonly view: ListNode) {}

  destroy(): void {
    if (this.viewabilityTimer !== null) clearTimeout(this.viewabilityTimer);
  }

  update(props: Record<string, any>, previous: Record<string, any>): void {
    // The core's rows reach past the view. Rows are clipped to it.
    this.view.style = { ...this.view.style, overflow: 'hidden' };
    if (
      !this.hasSettings ||
      props.overscan !== previous.overscan ||
      props.startReachedThreshold !== previous.startReachedThreshold ||
      props.endReachedThreshold !== previous.endReachedThreshold ||
      props.numberOfColumns !== previous.numberOfColumns ||
      props.inverted !== previous.inverted ||
      props.followAppends !== previous.followAppends ||
      props.horizontal !== previous.horizontal ||
      props.snapToItem !== previous.snapToItem ||
      props.snapAlignment !== previous.snapAlignment
    ) {
      this.hasSettings = true;
      this.core.setSettings({
        overscan: props.overscan ?? 1,
        startReachedThreshold: props.startReachedThreshold ?? 1,
        endReachedThreshold: props.endReachedThreshold ?? 1,
        numberOfColumns: props.numberOfColumns ?? 1,
        inverted: props.inverted === true,
        followAppends: props.followAppends === true,
        horizontal: props.horizontal === true,
        snapToItem: props.snapToItem === true,
        snapAlignment: props.snapAlignment ?? 0,
      });
      this.needsFrame = true;
    }
    if (props.stickyIndices !== previous.stickyIndices) {
      this.core.setStickyIndices(props.stickyIndices ?? []);
      this.needsFrame = true;
    }
    if (
      props.scrollIndex !== previous.scrollIndex &&
      props.scrollIndex !== undefined &&
      props.scrollIndex >= 0
    ) {
      this.pendingScrollIndex = props.scrollIndex;
      this.needsFrame = true;
    }
    if (
      props.viewableRules !== previous.viewableRules ||
      props.viewabilityInteracted !== previous.viewabilityInteracted ||
      props.viewableEventEnabled !== previous.viewableEventEnabled
    ) {
      this.viewabilityStale = true;
    }
    if (previous.refreshing === true && props.refreshing !== true) {
      // Nothing to wait for: a terminal draws no spinner to settle.
      this.view.dispatchEvent('onRefreshSettle', {});
    }
  }

  // region Commands

  command(command: string, args: unknown[]): void {
    this.syncKeys();
    switch (command) {
      case 'setStartReachedEnabled':
        this.startReachedEnabled = args[0] as boolean;
        return;
      case 'setEndReachedEnabled':
        this.endReachedEnabled = args[0] as boolean;
        return;
      case 'scrollToRow':
        this.core.scrollToRow(
          args[0] as number,
          args[1] as number,
          args[2] as number
        );
        break;
      case 'scrollToOffset':
        // An offset is the host's to write. The core hears of it as a move of the user's.
        this.offset = Math.round(
          Math.max(0, Math.min(args[0] as number, this.maxOffset()))
        );
        this.userScrolled = true;
        break;
      case 'scrollToEnd':
        this.core.scrollToEnd();
        break;
      case 'requestAnchorState': {
        const anchor = this.core.getAnchorState(this.offset);
        this.view.dispatchEvent('onAnchorState', {
          found: anchor !== null,
          key: anchor?.key ?? '',
          offset: anchor?.offset ?? 0,
        });
        return;
      }
      default:
        // flashScrollIndicators and closeSwipeActions have nothing to do here.
        return;
    }
    this.needsFrame = true;
    this.view.invalidate();
  }

  // endregion

  // region Input

  wheel(event: WheelEvent): boolean {
    const { props } = this.view;
    if (props.scrollEnabled === false) return false;
    const delta = props.horizontal
      ? event.deltaX || event.deltaY
      : event.deltaY;
    if (delta === 0) return false;
    const target = Math.max(0, Math.min(this.offset + delta, this.maxOffset()));
    const next = props.snapToItem
      ? this.core.nearestSnapOffset(target)
      : target;
    if (next === this.offset) return true;
    this.offset = next;
    this.userScrolled = true;
    this.wheelMoved = true;
    this.needsFrame = true;
    this.view.dispatchEvent('onScrollBeginDrag', this.scrollEvent());
    this.view.dispatchEvent('onScrollEndDrag', this.scrollEvent());
    this.view.invalidate();
    return true;
  }

  pointer(event: PointerEvent): boolean {
    if (event.kind === 'down' && this.view.props.focusable === true) {
      this.view.focus();
    }
    return false;
  }

  // endregion

  // region Layout

  paint(): void {
    const { view } = this;
    const { width, height } = view.frame;
    const horizontal = view.props.horizontal === true;
    const windowAlong = horizontal ? width : height;
    const windowCross = horizontal ? height : width;
    if (windowAlong <= 0 || windowCross <= 0) return;

    const children = this.childrenOf();
    const headerSize = this.sizeOf(children.header, horizontal);
    const footerSize = this.sizeOf(children.footer, horizontal);
    if (windowCross !== this.windowCross && this.windowCross > 0) {
      this.resetKeepingPosition();
    }
    if (
      windowAlong !== this.windowAlong ||
      windowCross !== this.windowCross ||
      headerSize !== this.headerSize ||
      footerSize !== this.footerSize
    ) {
      this.windowAlong = windowAlong;
      this.windowCross = windowCross;
      this.headerSize = headerSize;
      this.footerSize = footerSize;
      this.needsFrame = true;
    }
    this.syncKeys();
    if (this.pendingScrollIndex >= 0) {
      this.core.scrollToRow(this.pendingScrollIndex, 0, 0);
      this.pendingScrollIndex = -1;
    }

    const moved = this.offset !== this.sentOffset;
    const outsideBand =
      this.offset < this.bandLow || this.offset > this.bandHigh;
    let ran = false;
    for (let pass = 0; pass < MAX_PASSES_PER_PAINT; pass++) {
      // Rows that mounted since at a size the core does not have yet.
      const stale = this.markChangedRows(children, horizontal);
      if (!stale && !this.needsFrame && (pass > 0 || !outsideBand)) break;
      this.runPasses(children, horizontal);
      this.holdAnchor();
      ran = true;
    }
    if (this.needsFrame) this.view.invalidate();
    this.place(children, horizontal);
    this.sendEvents(ran || moved);
    this.sendMountedRange();
  }

  private childrenOf(): ListChildren {
    const result: ListChildren = {
      cells: new Map(),
      header: null,
      footer: null,
      empty: null,
      overlay: null,
    };
    for (const child of this.view.children) {
      if (child.type === CELL_VIEW) {
        result.cells.set(child.props.rowKey, child);
        continue;
      }
      switch (child.props.templateType) {
        case 'header':
          result.header = child;
          break;
        case 'footer':
          result.footer = child;
          break;
        case 'empty':
          result.empty = child;
          break;
        case 'sectionHeader':
          result.overlay = child;
          break;
      }
    }
    return result;
  }

  private sizeOf(node: ChildNode | null, horizontal: boolean): number {
    if (!node) return 0;
    return horizontal ? node.width : node.height;
  }

  /*
   * Drop all sizes after a width change and hold the row at the viewport start.
   */
  private resetKeepingPosition(): void {
    this.heldAnchor = this.core.getAnchorState(this.offset);
    this.core.resetKeepingPosition();
    this.bandLow = 1;
    this.bandHigh = 0;
    this.needsFrame = true;
  }

  /*
   * Every size is new after a reset, and the first passes take them in after the core's own landing
   * on the anchor, so the row is followed here until the sizes settle.
   */
  private holdAnchor(): void {
    const anchor = this.heldAnchor;
    if (anchor === null) return;
    const index = this.core.indexOfKey(anchor.key);
    if (index < 0 || index >= this.core.getRowCount()) {
      this.heldAnchor = null;
      return;
    }
    const target = Math.min(
      Math.round(this.core.getLeadingAt(index) + anchor.offset),
      this.maxOffset()
    );
    if (target !== this.offset) {
      this.offset = target;
      this.needsFrame = true;
    } else if (!this.needsFrame) {
      this.heldAnchor = null;
    }
  }

  private syncKeys(): void {
    const { keysStore, keysVersion } = this.view.props;
    if (
      !keysStore ||
      (keysStore === this.adoptedStore && keysVersion === this.adoptedVersion)
    ) {
      return;
    }
    if (this.core.adoptKeys(keysStore, keysVersion)) {
      this.adoptedStore = keysStore;
      this.adoptedVersion = keysVersion;
      this.needsFrame = true;
      this.viewabilityStale = true;
    }
  }

  private runPasses(children: ListChildren, horizontal: boolean): void {
    const phase = this.userScrolled ? PHASE_DRAGGING : PHASE_IDLE;
    const userScrolled = this.userScrolled;
    this.userScrolled = false;
    this.needsFrame = false;
    const result = this.core.runPasses(
      {
        offset: this.offset,
        windowAlong: this.windowAlong,
        windowCross: this.windowCross,
        headerSize: this.headerSize,
        footerSize: this.footerSize,
        phase,
        userScrolled,
      },
      (_index, key) => {
        const cell = children.cells.get(key);
        return cell ? this.sizeOf(cell, horizontal) : 1;
      }
    );
    const previousOffset = this.offset;
    const previousContent = this.content;
    this.applyPassResult(result);
    /*
     * Go again for the pass at rest after a wheel notch, which ends the notch's gesture, for a
     * correction still landing, and when this pass moved things: sizes it took in can move a row a
     * scroll command already reached.
     */
    if (
      userScrolled ||
      result.settling ||
      this.offset !== previousOffset ||
      this.content !== previousContent
    ) {
      this.needsFrame = true;
    }
  }

  private applyPassResult(result: PassResult): void {
    this.content = result.content;
    this.footerStart = result.footerStart;
    this.bandLow = result.bandLow;
    this.bandHigh = result.bandHigh;
    this.offset = Math.round(result.offset);
    this.offset = Math.min(this.offset, this.maxOffset());
    const { props } = this.view;
    if (
      result.reachedStart &&
      this.startReachedEnabled &&
      props.onStartReached
    ) {
      this.view.dispatchEvent('onStartReached', {});
    }
    if (result.reachedEnd && this.endReachedEnabled && props.onEndReached) {
      this.view.dispatchEvent('onEndReached', {});
    }
  }

  private maxOffset(): number {
    return Math.max(0, Math.round(this.content) - this.windowAlong);
  }

  /*
   * Marks the mounted rows whose size differs from the core's. Returns whether any does.
   */
  private markChangedRows(
    children: ListChildren,
    horizontal: boolean
  ): boolean {
    const count = this.core.getRowCount();
    let changed = false;
    for (const [key, cell] of children.cells) {
      const index = this.core.indexOfKey(key);
      if (index < 0 || index >= count) continue;
      if (this.core.getExtentAt(index) !== this.sizeOf(cell, horizontal)) {
        this.core.markRemeasure(index);
        changed = true;
      }
    }
    return changed;
  }

  /*
   * Puts every child where the core says, at the offset. Nothing else positions them.
   */
  private place(children: ListChildren, horizontal: boolean): void {
    const { offset } = this;
    const count = this.core.getRowCount();
    for (const [key, cell] of children.cells) {
      const index = this.core.indexOfKey(key);
      if (index < 0 || index >= count) {
        this.placeChild(cell, OFFSCREEN, OFFSCREEN);
        continue;
      }
      const rect = this.core.getRowRect(index);
      if (horizontal) {
        this.placeChild(cell, rect.x - offset, rect.y);
      } else {
        this.placeChild(cell, rect.x, rect.y - offset);
      }
    }
    const { props } = this.view;
    const { header, footer, empty, overlay } = children;
    const sticky = this.core.stickyTranslations({
      offset,
      windowSize: this.windowAlong,
      contentSize: this.content,
      hasHeader: header !== null,
      headerSize: this.headerSize,
      stickyHeader: props.stickyHeader === true,
      autoHideHeader: props.autoHideHeader === true,
      hasFooter: footer !== null,
      footerSize: this.footerSize,
      footerStart: this.footerStart,
      stickyFooter: props.stickyFooter === true,
      autoHideFooter: props.autoHideFooter === true,
      accumulate: this.wheelMoved,
    });
    this.wheelMoved = false;
    if (header) {
      if (props.stickyHeader || props.autoHideHeader) this.paintAbove(header);
      this.placeAlong(header, sticky.header - offset, horizontal);
    }
    if (footer) {
      if (props.stickyFooter || props.autoHideFooter) this.paintAbove(footer);
      this.placeAlong(
        footer,
        this.footerStart + sticky.footer - offset,
        horizontal
      );
    }
    if (empty) {
      this.placeAlong(empty, this.headerSize - offset, horizontal);
    }
    if (overlay) {
      const active = this.core.activeStickyIndex(offset);
      this.placeAlong(
        overlay,
        active < 0
          ? OFFSCREEN
          : this.core.stickyLeading(active, offset) - offset,
        horizontal
      );
    }
  }

  /*
   * A pinned template comes before the rows in the tree, and they scroll under it.
   */
  private paintAbove(node: ChildNode): void {
    if (node.style.zIndex === undefined) {
      node.style = { ...node.style, zIndex: 1 };
    }
  }

  private placeAlong(
    node: ChildNode,
    along: number,
    horizontal: boolean
  ): void {
    this.placeChild(node, horizontal ? along : 0, horizontal ? 0 : along);
  }

  /*
   * Moves a child so it sits at x and y in the view. A child in the flow, as a template is, has its
   * Yoga position added to where the flow put it. What this leaves in Yoga holds at the next
   * layout, and the sync moves the subtree for this paint.
   */
  private placeChild(node: ChildNode, left: number, top: number): void {
    const { yoga } = node;
    if (!yoga) return;
    // A row's estimate can be a fraction of a cell, which has no place to show.
    const x = Math.round(left);
    const y = Math.round(top);
    const previous = this.placements.get(node) ?? { x: 0, y: 0 };
    const nextX = x - (node.left - previous.x);
    const nextY = y - (node.top - previous.y);
    if (nextX !== previous.x || nextY !== previous.y) {
      yoga.setPosition(EDGE_LEFT, nextX);
      yoga.setPosition(EDGE_TOP, nextY);
      this.placements.set(node, { x: nextX, y: nextY });
    }
    const { frame } = this.view;
    node.syncLayout(frame.x + x - node.left, frame.y + y - node.top, NO_EVENTS);
  }

  // endregion

  // region Events

  private scrollEvent(): object {
    const { horizontal } = this.view.props;
    const { offset, content, windowAlong, windowCross } = this;
    const x = horizontal ? offset : 0;
    const y = horizontal ? 0 : offset;
    return {
      contentOffsetX: x,
      contentOffsetY: y,
      contentOffset: { x, y },
      contentSize: horizontal
        ? { width: content, height: windowCross }
        : { width: windowCross, height: content },
      layoutMeasurement: horizontal
        ? { width: windowAlong, height: windowCross }
        : { width: windowCross, height: windowAlong },
      velocity: { x: 0, y: 0 },
      contentInset: { top: 0, left: 0, bottom: 0, right: 0 },
      zoomScale: 1,
    };
  }

  private sendEvents(geometryChanged: boolean): void {
    const { view, offset, content } = this;
    const { props } = view;
    if (offset !== this.sentOffset) {
      this.sentOffset = offset;
      if (props.scrollEventEnabled) {
        view.dispatchEvent('onScroll', this.scrollEvent());
      }
    }
    if (content !== this.sentContent) {
      this.sentContent = content;
      if (props.contentSizeEventEnabled) {
        const { windowCross } = this;
        view.dispatchEvent(
          'onContentSizeChange',
          props.horizontal
            ? { width: content, height: windowCross }
            : { width: windowCross, height: content }
        );
      }
    }
    if (props.inverted && props.onAtEndChange) {
      const atEnd = offset >= this.maxOffset() - 1;
      if (atEnd !== this.sentAtEnd) {
        this.sentAtEnd = atEnd;
        view.dispatchEvent('onAtEndChange', { atEnd });
      }
    }
    if (geometryChanged) this.viewabilityStale = true;
    if (props.viewableEventEnabled && this.viewabilityStale) {
      this.viewabilityStale = false;
      this.sendViewability();
    }
  }

  /*
   * Whether a row is viewable under a rule, the core's reading of a threshold: its own share of
   * the row, or with coverage its share of the viewport.
   */
  private isViewable(
    index: number,
    threshold: number,
    coverage: boolean
  ): boolean {
    const start = this.core.getLeadingAt(index);
    const size = this.core.getExtentAt(index);
    const shown =
      Math.min(start + size, this.offset + this.windowAlong) -
      Math.max(start, this.offset);
    if (shown <= 0) return false;
    const needed = coverage
      ? Math.min(size, threshold * this.windowAlong)
      : threshold * Math.min(size, this.windowAlong);
    return shown >= needed;
  }

  private sendViewability(): void {
    const { props } = this.view;
    const visible = this.core.getVisibleRange();
    const low = visible?.low ?? -1;
    const high = visible?.high ?? -1;
    const active = this.core.activeStickyIndex(this.offset);
    const rules: ReadonlyArray<number> = props.viewableRules ?? [];
    const timings: ReadonlyArray<number> = props.viewableTimings ?? [];
    const now = Date.now();
    const changes: {
      config: number;
      addedKeys: string[];
      removedKeys: string[];
    }[] = [];
    let wait = false;
    // The first rule is any overlap, for the pinned header. One rule per config follows.
    for (let config = 0; (config + 1) * 2 < rules.length; config++) {
      const threshold = rules[(config + 1) * 2]!;
      const coverage = rules[(config + 1) * 2 + 1] === 1;
      const viewable: string[] = [];
      for (let index = low; low >= 0 && index <= high; index++) {
        if (this.isViewable(index, threshold, coverage)) {
          viewable.push(this.core.getKeyAt(index));
        }
      }
      const update = this.core.updateViewability(
        config,
        timings[config * 2] ?? 0,
        timings[config * 2 + 1] === 1,
        props.viewabilityInteracted === true,
        viewable,
        now
      );
      wait = wait || update.waiting;
      if (update.added.length > 0 || update.removed.length > 0) {
        changes.push({
          config,
          addedKeys: update.added,
          removedKeys: update.removed,
        });
      }
    }
    if (wait && this.viewabilityTimer === null) {
      this.viewabilityTimer = setTimeout(() => {
        this.viewabilityTimer = null;
        this.viewabilityStale = true;
        this.view.invalidate();
      }, VIEWABILITY_POLL_MS);
    }
    if (
      changes.length > 0 ||
      low !== this.sentVisibleLow ||
      high !== this.sentVisibleHigh ||
      active !== this.sentActiveSticky
    ) {
      this.sentVisibleLow = low;
      this.sentVisibleHigh = high;
      this.sentActiveSticky = active;
      this.view.dispatchEvent('onViewabilityChange', {
        activeStickyIndex: active,
        visibleLow: low,
        visibleHigh: high,
        changes,
      });
    }
  }

  /*
   * Sends the next rows for React to mount when the rows on screen, with half the overscan, leave
   * the mounted ones, or when far more are mounted than the overscan asks. One range is in flight
   * at a time. A scroll command's rows wait for this report to say it landed.
   */
  private sendMountedRange(): void {
    const { props } = this.view;
    const visible = this.core.getVisibleRange();
    const count = this.core.getRowCount();
    if (props.mountedGeneration !== this.sentGeneration) {
      this.sentGeneration = props.mountedGeneration ?? 0;
      this.sentSequence = props.mountedSequence ?? 0;
    }
    const inFlight = this.sentSequence > (props.mountedSequence ?? 0);
    if (inFlight) return;

    const seedPending = props.seedPending === true;
    const low: number = props.mountedLow ?? -1;
    const high: number = props.mountedHigh ?? -1;
    if (!visible || count === 0) {
      if (seedPending) this.sendLanded();
      return;
    }
    const overscan: number = props.mountOverscanRows ?? 4;
    const nextLow = Math.max(0, visible.low - overscan);
    const nextHigh = Math.min(count - 1, visible.high + overscan);
    const margin = Math.floor(overscan / 2);
    const needed =
      low < 0 ||
      Math.max(0, visible.low - margin) < low ||
      Math.min(count - 1, visible.high + margin) > high;
    const oversized = high - low - (nextHigh - nextLow) > overscan;
    if (needed || oversized) {
      this.sentSequence =
        Math.max(this.sentSequence, props.mountedSequence ?? 0) + 1;
      this.view.dispatchEvent('onMountedRangeChange', {
        lowKey: this.core.getKeyAt(nextLow),
        highKey: this.core.getKeyAt(nextHigh),
        lowIndex: nextLow,
        highIndex: nextHigh,
        lowAtStart: nextLow === 0,
        highAtEnd: nextHigh === count - 1,
        sequence: this.sentSequence,
        generation: this.sentGeneration,
        landed: false,
      });
    } else if (seedPending) {
      this.sendLanded();
    }
    this.sendPrefetch(low, high);
  }

  private sendLanded(): void {
    this.view.dispatchEvent('onMountedRangeChange', {
      lowKey: '',
      highKey: '',
      lowIndex: -1,
      highIndex: -1,
      lowAtStart: false,
      highAtEnd: false,
      sequence: 0,
      generation: this.sentGeneration,
      landed: true,
    });
  }

  private sendPrefetch(low: number, high: number): void {
    const { props } = this.view;
    if (!(props.prefetchRows > 0) || !props.onPrefetchChange || low < 0) return;
    const change = this.core.updatePrefetch(low, high);
    if (!change) return;
    this.view.dispatchEvent('onPrefetchChange', {
      prefetch: change.prefetch,
      cancel: change.cancel,
      prefetchKeys: change.prefetch.map((index) => this.core.getKeyAt(index)),
      cancelKeys: change.cancel.map((index) => this.core.getKeyAt(index)),
    });
  }

  // endregion
}

const controllers = new WeakMap<NativeView, ListViewController>();

function controllerOf(view: NativeView): ListViewController {
  return controllers.get(view)!;
}

/*
 * Registers the three views and the module. A consumer gets this by importing the package.
 */
registerViewManager('ShadowListView', {
  create(view) {
    controllers.set(view, new ListViewController(view as ListNode));
  },
  update(view, props, previous) {
    controllerOf(view).update(props, previous);
  },
  paint(view) {
    controllerOf(view).paint();
  },
  command(view, command, args) {
    controllerOf(view).command(command, args);
  },
  wheel(view, event) {
    return controllerOf(view).wheel(event);
  },
  pointer(view, event) {
    return controllerOf(view).pointer(event);
  },
  destroy(view) {
    controllerOf(view).destroy();
  },
});

// Rows and templates draw only their React children.
registerViewManager(CELL_VIEW, { paint() {} });
registerViewManager('ShadowListTemplateView', { paint() {} });

registerNativeModule('ShadowListStore', shadowListStoreModule());
