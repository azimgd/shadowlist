import { ShadowListKitAnchorState } from './ShadowListKitAnchorState.js';
import {
  ShadowListKitCore,
  isShadowListKitLoaded,
  loadShadowListKit,
} from './ShadowListKitCore.js';
import { ShadowListKitListCell } from './ShadowListKitListCell.js';
import type { ShadowListKitListChanges } from './ShadowListKitListChanges.js';
import { ShadowListKitListData } from './ShadowListKitListData.js';
import { ShadowListKitScrollGesture } from './ShadowListKitScrollGesture.js';
import { ShadowListKitStickyPinning } from './ShadowListKitStickyPinning.js';

/*
 * A measured size in whole CSS pixels, like the native kits' sizes. Row edges, the content size
 * and the offset then stay whole and the browser never snaps one of them on its own.
 */
function wholePixels(size: number): number {
  return Math.ceil(size - 0.01);
}

/*
 * What the list shows. keyForItem is a stable identity for the row. Sizes, cells and the
 * scroll position follow keys across reloadData. A prepend or an insert above keeps what is on
 * screen in place. A row whose content changed under the same key needs reloadItems.
 *
 * sizeForItem gives sizes without cells, like from a layout computed ahead. Otherwise every row
 * is measured once through its cell. reconfigureCell updates a shown cell for a payload given to
 * reloadItems and returns true, or false to have the row reloaded in full instead.
 * contentVersionForItem changes with an item's content, like a revision or a hash. applyChanges
 * reloads the rows whose value changed.
 */
export interface ShadowListKitListViewDataSource {
  numberOfItems(listView: ShadowListKitListView): number;
  keyForItem(listView: ShadowListKitListView, index: number): string;
  cellForItem(
    listView: ShadowListKitListView,
    index: number
  ): ShadowListKitListCell;
  sizeForItem?(
    listView: ShadowListKitListView,
    index: number,
    crossSize: number
  ): number;
  reconfigureCell?(
    listView: ShadowListKitListView,
    cell: ShadowListKitListCell,
    index: number,
    payload: unknown
  ): boolean;
  contentVersionForItem?(
    listView: ShadowListKitListView,
    index: number
  ): number | string;
}

/*
 * Hears what the list does. didEndScrolling fires once the list came to rest after a scroll or
 * an animated command, on a row edge when snapToItem is set. didReachStart and didReachEnd fire
 * when the scroll position came within startReachedThreshold or endReachedThreshold of an edge.
 */
export interface ShadowListKitListViewDelegate {
  willDisplayCell?(
    listView: ShadowListKitListView,
    cell: ShadowListKitListCell,
    index: number
  ): void;
  didEndDisplayingCell?(
    listView: ShadowListKitListView,
    cell: ShadowListKitListCell,
    index: number
  ): void;
  didScroll?(listView: ShadowListKitListView): void;
  didEndScrolling?(listView: ShadowListKitListView): void;
  didReachStart?(listView: ShadowListKitListView): void;
  didReachEnd?(listView: ShadowListKitListView): void;
}

/*
 * Hears which items the list will soon show, to load what their cells need ahead. Prefetched
 * items are the ones in the core's measured window, an overscan past the viewport, that have no
 * cell yet. A prefetched item that leaves the window before it shows is cancelled.
 */
export interface ShadowListKitListViewPrefetchDataSource {
  prefetchItems(listView: ShadowListKitListView, indices: number[]): void;
  cancelPrefetchingForItems?(
    listView: ShadowListKitListView,
    indices: number[]
  ): void;
}

/*
 * A virtualized list on the shadowlist core, as the custom element <shadowlist-kit-list-view>.
 * Rows are placed by the core, which keeps the visible content still while rows are measured,
 * inserted or removed. Measurement, offset corrections and mounting all happen in one layout
 * pass before the browser paints. A scroll frame that stays inside the core's offset band does
 * no core work at all. Sizes are in CSS pixels. The element needs a size from its styles.
 */
export class ShadowListKitListView extends HTMLElement {
  static readonly TAG_NAME = 'shadowlist-kit-list-view';

  dataSource: ShadowListKitListViewDataSource | null = null;
  delegate: ShadowListKitListViewDelegate | null = null;
  prefetchDataSource: ShadowListKitListViewPrefetchDataSource | null = null;

  private readonly properties = {
    inverted: false,
    followAppends: false,
    horizontal: false,
    numberOfColumns: 1,
    estimatedItemSize: 120,
    overscan: 1,
    mountOverscan: 0.5,
    startReachedThreshold: 1,
    endReachedThreshold: 1,
    snapToItem: false,
    snapAlignment: 0,
    stickyIndices: [] as number[],
    headerView: null as HTMLElement | null,
    footerView: null as HTMLElement | null,
  };

  /*
   * Created on first use and destroyed when the list disconnects. A list connected again gets a
   * new core with the same settings and keys. Rows are measured again. A list that never
   * connects leaves its core to the core's reclaimer, which frees the peer once the list is
   * collected.
   */
  private coreOrNull: ShadowListKitCore | null = null;

  private readonly gesture = new ShadowListKitScrollGesture(this);
  private readonly stickyPinning = new ShadowListKitStickyPinning(this);
  private readonly data = new ShadowListKitListData(this);

  /*
   * The element the rows are placed in. Its size along the scroll axis is the content size.
   */
  private readonly content = document.createElement('div');

  /*
   * Mounted cells by key. A cell follows its key across inserts above it.
   */
  readonly mounted = new Map<string, ShadowListKitListCell>();
  private mountGeneration = 0;

  /*
   * What the last mount pass covered. The pass is skipped while all of it holds.
   */
  private mountedLow = -1;
  private mountedHigh = -1;
  private mountedSticky = -1;
  private mountedGeometry = -1;
  private mountedStructure = 0;
  private structureVersion = 0;

  /*
   * The offset of the last mount pass. The step since then sets how far ahead rows mount.
   */
  private mountedOffset = 0;

  private readonly cellFactories = new Map<
    string,
    () => ShadowListKitListCell
  >();
  private readonly reusePool = new Map<string, ShadowListKitListCell[]>();

  /*
   * The rows the core keeps measured and their frames, four values each.
   */
  private windowLow = -1;
  private windowHigh = -1;
  private windowFrames = new Float64Array(256);
  private windowGeometry = -1;
  private readonly scratchFrame = new Float64Array(4);

  /*
   * The geometry version of the core's last pass. Sticky pinning copies its frames again when
   * it changes.
   */
  frameGeometry = 0;

  /*
   * The band of offsets where the core has nothing to do.
   */
  private bandLow = 1;
  private bandHigh = 0;
  private needsFrame = true;

  /*
   * Geometry the core last ran with.
   */
  windowAlong = 0;
  windowCross = 0;
  private headerSize = 0;
  private footerSize = 0;
  private templatesChanged = true;
  contentAlong = 0;

  /*
   * The last offset seen, to tell the reader's scrolling from our own writes.
   */
  private previousOffset = 0;
  private userScrolled = false;

  private inLayoutPass = false;
  private frameScheduled = false;
  private reachedStart = false;
  private reachedEnd = false;

  private pendingAnchor: ShadowListKitAnchorState | null = null;

  /*
   * Sees the list's own size and the header and footer sizes change.
   */
  private readonly resizeObserver = new ResizeObserver((entries) => {
    for (const entry of entries) {
      if (entry.target !== this) this.templatesChanged = true;
    }
    this.invalidateFrame();
  });

  constructor() {
    super();
    const style = this.content.style;
    style.position = 'relative';
    style.overflow = 'clip';
  }

  connectedCallback(): void {
    const style = this.style;
    if (!style.display) style.display = 'block';
    style.position = 'relative';
    style.overflowAnchor = 'none';
    this.applyAxisStyles();
    if (!this.getAttribute('role')) this.setAttribute('role', 'list');
    if (this.content.parentNode !== this) this.prepend(this.content);
    this.addEventListener('scroll', this.onScroll, { passive: true });
    this.gesture.attach();
    this.resizeObserver.observe(this);
    for (const view of [this.headerView, this.footerView]) {
      if (view) this.resizeObserver.observe(view);
    }
    if (!isShadowListKitLoaded()) {
      loadShadowListKit().then(() => this.invalidateFrame());
      return;
    }
    this.restorePendingAnchor();
    this.invalidateFrame();
  }

  disconnectedCallback(): void {
    this.removeEventListener('scroll', this.onScroll);
    this.gesture.detach();
    this.resizeObserver.disconnect();
    /*
     * The browser drops the scroll position of a removed element. A list moved in the page
     * lands on the same row when it connects again.
     */
    const anchor = this.coreOrNull?.anchor(this.previousOffset);
    if (anchor && !this.pendingAnchor) {
      this.pendingAnchor = new ShadowListKitAnchorState(
        anchor.key,
        anchor.offset
      );
    }
    this.destroyCore();
  }

  // region Axis

  /*
   * The scroll offset along the axis, in CSS pixels.
   */
  get offset(): number {
    return this.horizontal ? this.scrollLeft : this.scrollTop;
  }

  get maxOffset(): number {
    return Math.max(0, this.contentAlong - this.windowAlong);
  }

  /*
   * Move to an offset the list asked for itself.
   */
  private writeOffset(target: number): void {
    const value = Math.round(target);
    this.previousOffset = value;
    if (this.horizontal) this.scrollLeft = value;
    else this.scrollTop = value;
  }

  /*
   * Something other than the list moved it since the list last wrote or saw the offset.
   */
  get movedElsewhere(): boolean {
    return Math.abs(this.offset - this.previousOffset) >= 1;
  }

  /*
   * One step of an animated scroll, laid out right away.
   */
  scrollToOffset(value: number): void {
    this.writeOffset(Math.min(Math.max(value, 0), this.maxOffset));
    this.layoutPass();
  }

  private scrollPhase(): number {
    if (this.gesture.isTracking) return ShadowListKitCore.PHASE_DRAGGING;
    if (this.gesture.isScrolling) return ShadowListKitCore.PHASE_SETTLING;
    return ShadowListKitCore.PHASE_IDLE;
  }

  /*
   * Only the scroll axis scrolls. The scrollbar gutter stays, which keeps the cross size from
   * changing when the scrollbar shows. The content keeps its size along the axis. A list
   * connected again would otherwise lose its scroll range, and a horizontal list its
   * scrollbar and with it the cross size.
   */
  private applyAxisStyles(): void {
    const style = this.style;
    style.overflowX = this.horizontal ? 'auto' : 'hidden';
    style.overflowY = this.horizontal ? 'hidden' : 'auto';
    style.scrollbarGutter = 'stable';
    const along = this.contentAlong > 0 ? `${this.contentAlong}px` : '';
    this.content.style.width = this.horizontal ? along : '100%';
    this.content.style.height = this.horizontal ? '100%' : along;
  }

  // endregion

  // region Properties

  /*
   * A chat style list that opens at its end and keeps the end in view while the reader is
   * there. Rows stay in data order, oldest first.
   */
  get inverted(): boolean {
    return this.properties.inverted;
  }
  set inverted(value: boolean) {
    this.properties.inverted = value;
    this.sendSettings();
  }

  /*
   * With inverted, rows appended while the reader rests at the end scroll into view, like an
   * assistant reply. Off by default, which keeps the visible rows in place.
   */
  get followAppends(): boolean {
    return this.properties.followAppends;
  }
  set followAppends(value: boolean) {
    this.properties.followAppends = value;
    this.sendSettings();
  }

  get horizontal(): boolean {
    return this.properties.horizontal;
  }
  set horizontal(value: boolean) {
    this.properties.horizontal = value;
    this.applyAxisStyles();
    this.sendSettings();
  }

  /*
   * Columns of a grid. Row i goes into column i % numberOfColumns and each column stacks its
   * own rows.
   */
  get numberOfColumns(): number {
    return this.properties.numberOfColumns;
  }
  set numberOfColumns(value: number) {
    this.properties.numberOfColumns = Math.max(1, Math.floor(value));
    this.sendSettings();
  }

  /*
   * Size along the scroll axis assumed for rows not measured yet, in CSS pixels.
   */
  get estimatedItemSize(): number {
    return this.properties.estimatedItemSize;
  }
  set estimatedItemSize(value: number) {
    this.properties.estimatedItemSize = value;
    this.sendSettings();
  }

  /*
   * How far past the viewport rows are measured, in viewport sizes. Default 1.
   */
  get overscan(): number {
    return this.properties.overscan;
  }
  set overscan(value: number) {
    this.properties.overscan = value;
    this.sendSettings();
  }

  /*
   * How far past the viewport cells are mounted, in viewport sizes. Default 0.5.
   */
  get mountOverscan(): number {
    return this.properties.mountOverscan;
  }
  set mountOverscan(value: number) {
    this.properties.mountOverscan = value;
    this.invalidateFrame();
  }

  /*
   * Distances to an edge, in viewport sizes, that fire the reached callbacks. Default 1.
   */
  get startReachedThreshold(): number {
    return this.properties.startReachedThreshold;
  }
  set startReachedThreshold(value: number) {
    this.properties.startReachedThreshold = value;
    this.sendSettings();
  }

  get endReachedThreshold(): number {
    return this.properties.endReachedThreshold;
  }
  set endReachedThreshold(value: number) {
    this.properties.endReachedThreshold = value;
    this.sendSettings();
  }

  /*
   * Rest the scroll position on a row edge. Alignment 0 start, 1 center, 2 end.
   */
  get snapToItem(): boolean {
    return this.properties.snapToItem;
  }
  set snapToItem(value: boolean) {
    this.properties.snapToItem = value;
    this.sendSettings();
  }

  get snapAlignment(): number {
    return this.properties.snapAlignment;
  }
  set snapAlignment(value: number) {
    this.properties.snapAlignment = value;
    this.sendSettings();
  }

  /*
   * Items that stick to the top of the viewport once scrolled past.
   */
  get stickyIndices(): ReadonlyArray<number> {
    return this.properties.stickyIndices;
  }
  set stickyIndices(value: ReadonlyArray<number>) {
    this.properties.stickyIndices = [...value].sort((a, b) => a - b);
    this.updateStickyRows();
    this.invalidateFrame();
  }

  /*
   * Elements before the first row and after the last. Their size along the scroll axis is
   * their natural size at the list's cross size, followed as it changes.
   */
  get headerView(): HTMLElement | null {
    return this.properties.headerView;
  }
  set headerView(value: HTMLElement | null) {
    this.replaceTemplate(this.properties.headerView, value);
    this.properties.headerView = value;
  }

  get footerView(): HTMLElement | null {
    return this.properties.footerView;
  }
  set footerView(value: HTMLElement | null) {
    this.replaceTemplate(this.properties.footerView, value);
    this.properties.footerView = value;
  }

  get core(): ShadowListKitCore {
    return this.coreOrNull ?? this.createCore();
  }

  /*
   * The core when it exists, for work that has nothing to do without one.
   */
  get liveCore(): ShadowListKitCore | null {
    return this.coreOrNull;
  }

  private sendSettings(): void {
    if (this.coreOrNull) this.applySettings(this.coreOrNull);
    this.invalidateFrame();
  }

  private applySettings(target: ShadowListKitCore): void {
    const properties = this.properties;
    target.setSettings(
      properties.estimatedItemSize,
      properties.overscan,
      properties.startReachedThreshold,
      properties.endReachedThreshold,
      properties.numberOfColumns,
      properties.inverted,
      properties.followAppends,
      properties.horizontal,
      properties.snapToItem,
      properties.snapAlignment
    );
  }

  /*
   * A new core with this list's settings, sticky rows and keys. Every copy of its output here
   * is dropped and the next layout pass runs it.
   */
  private createCore(): ShadowListKitCore {
    const created = new ShadowListKitCore((row, crossSize) =>
      this.measureItem(row, crossSize)
    );
    this.coreOrNull = created;
    this.applySettings(created);
    if (this.stickyPinning.stickyRows.length > 0) {
      created.setStickyIndices(this.stickyPinning.stickyRows);
    }
    if (this.keys.length > 0) {
      created.replaceKeys(0, 0, this.keys, 0, this.keys.length);
    }
    this.windowLow = -1;
    this.windowHigh = -1;
    this.windowGeometry = -1;
    this.stickyPinning.invalidateFrames();
    this.bandLow = 1;
    this.bandHigh = 0;
    this.mountedLow = -1;
    this.needsFrame = true;
    ++this.structureVersion;
    return created;
  }

  private destroyCore(): void {
    this.coreOrNull?.destroy();
    this.coreOrNull = null;
  }

  private replaceTemplate(
    previous: HTMLElement | null,
    next: HTMLElement | null
  ): void {
    if (previous) {
      this.resizeObserver.unobserve(previous);
      previous.remove();
    }
    if (next) {
      next.style.position = 'absolute';
      next.style.left = '0';
      next.style.top = '0';
      this.content.prepend(next);
      if (this.isConnected) this.resizeObserver.observe(next);
    }
    this.templatesChanged = true;
    this.invalidateFrame();
  }

  /*
   * Run the core on the next animation frame even inside the band.
   */
  invalidateFrame(): void {
    this.needsFrame = true;
    if (this.frameScheduled) return;
    this.frameScheduled = true;
    requestAnimationFrame(() => {
      this.frameScheduled = false;
      // A list disconnected meanwhile dropped its core. The next connect lays out again.
      if (this.isConnected) this.layoutPass();
    });
  }

  // endregion

  // region Cells

  registerCell(identifier: string, factory: () => ShadowListKitListCell): void {
    this.cellFactories.set(identifier, factory);
  }

  dequeueReusableCell<T extends ShadowListKitListCell>(identifier: string): T {
    const cell = this.reusePool.get(identifier)?.pop();
    if (cell) {
      cell.prepareForReuse();
      return cell as T;
    }
    const factory = this.cellFactories.get(identifier);
    const created = factory ? factory() : new ShadowListKitListCell(identifier);
    this.content.append(created.element);
    return created as T;
  }

  recycleCell(cell: ShadowListKitListCell): void {
    const index = cell.index;
    cell.element.style.visibility = 'hidden';
    this.stickyPinning.release(cell);
    cell.index = ShadowListKitListCell.NO_INDEX;
    if (index >= 0) this.delegate?.didEndDisplayingCell?.(this, cell, index);
    const identifier = cell.reuseIdentifier;
    if (identifier == null) {
      cell.element.remove();
      return;
    }
    let pool = this.reusePool.get(identifier);
    if (!pool) {
      pool = [];
      this.reusePool.set(identifier, pool);
    }
    pool.push(cell);
  }

  /*
   * The cell of a row from the data source.
   */
  private makeCell(row: number): ShadowListKitListCell {
    const cell = this.dataSource!.cellForItem(this, row);
    if (cell.element.parentNode !== this.content) {
      this.content.append(cell.element);
    }
    cell.index = row;
    return cell;
  }

  get keys(): ReadonlyArray<string> {
    return this.data.keys;
  }

  get itemCount(): number {
    return this.data.itemCount;
  }

  /*
   * The mounted cell of a row, or null.
   */
  mountedCell(row: number): ShadowListKitListCell | null {
    const key = this.keys[row];
    return key === undefined ? null : (this.mounted.get(key) ?? null);
  }

  // endregion

  // region Data

  /*
   * Read the row count and keys again. Rows keep their sizes and cells by key, and the visible
   * content stays in place. Cells of surviving keys are not configured again. Only the keys
   * between the unchanged rows at both ends go to the core.
   */
  reloadData(): void {
    this.data.reloadData();
  }

  /*
   * Rows were inserted at these positions of the new data, which the data source already
   * reflects. Only the new keys are read. An insert past the end goes at the end.
   */
  insertItems(indices: ReadonlyArray<number>): void {
    this.data.insertItems(indices);
  }

  /*
   * Rows were deleted at these positions of the old data. A delete past the end is dropped.
   */
  deleteItems(indices: ReadonlyArray<number>): void {
    this.data.deleteItems(indices);
  }

  /*
   * The rows' content changed under the same keys. Visible cells are configured again and every
   * listed row is measured again. With a payload the data source's reconfigureCell can update a
   * shown cell instead.
   */
  reloadItems(indices: ReadonlyArray<number>, payload: unknown = null): void {
    this.data.reloadItems(indices, payload);
  }

  /*
   * An item moved, after the data source reflects it.
   */
  moveItem(index: number, newIndex: number): void {
    this.data.moveItem(index, newIndex);
  }

  /*
   * Inserts, deletes, moves and reloads made in updates land together in one layout. Deletes,
   * reloads and move sources are indices in the data before, inserts and move destinations in
   * the data after. A batch that does not add up reloads everything. completion runs once the
   * change is laid out.
   */
  performBatchUpdates(updates: () => void, completion?: () => void): void {
    this.data.performBatchUpdates(updates, completion);
  }

  /*
   * The data source already shows the new data. Read every key, work out the inserts, deletes
   * and moves against the keys held with the core's diffKeys, reload the rows whose content
   * version changed, and return what changed.
   */
  applyChanges(): ShadowListKitListChanges {
    return this.data.applyChanges();
  }

  /*
   * The data changed. Sticky rows follow the data and a waiting saved position lands once its
   * row is there.
   */
  structureChanged(): void {
    ++this.structureVersion;
    if (this.stickyIndices.length > 0) this.updateStickyRows();
    this.restorePendingAnchor();
    this.invalidateFrame();
  }

  private updateStickyRows(): void {
    const rows = this.stickyIndices.filter((index) => index < this.itemCount);
    if (this.stickyPinning.updateStickyRows(rows)) this.mountedLow = -1;
  }

  // endregion

  // region Layout

  private onScroll = (): void => {
    if (Math.abs(this.offset - this.previousOffset) >= 1) {
      // The page or the reader moved the list. An animation still running stops there.
      if (this.gesture.isAnimating) this.stopScrolling();
      this.gesture.userScrolled();
    }
    this.layoutPass();
    this.delegate?.didScroll?.(this);
  };

  /*
   * One pass runs the core when the offset left the band, then mounts. Everything lands before
   * the browser paints.
   */
  layoutPass(): void {
    if (this.inLayoutPass || !this.isConnected || !isShadowListKitLoaded()) {
      return;
    }
    this.inLayoutPass = true;
    try {
      const geometryChanged = this.readGeometry();
      this.trackUserScroll();
      if (this.windowAlong > 0 && this.windowCross > 0) {
        if (this.needsFrame || geometryChanged || !this.inBand(this.offset)) {
          this.runPasses();
        }
        this.layoutTemplates();
        this.mountCells();
      }
    } finally {
      this.inLayoutPass = false;
    }
    this.dispatchReached();
  }

  private inBand(value: number): boolean {
    return this.bandLow <= value && value <= this.bandHigh;
  }

  /*
   * Read the viewport and template sizes. Returns whether any changed since the last pass.
   */
  private readGeometry(): boolean {
    const along = this.horizontal ? this.clientWidth : this.clientHeight;
    const cross = this.horizontal ? this.clientHeight : this.clientWidth;
    if (cross !== this.windowCross && this.windowCross > 0) {
      this.resetKeepingPosition();
    }
    let header = this.headerSize;
    let footer = this.footerSize;
    if (this.templatesChanged || cross !== this.windowCross) {
      this.templatesChanged = false;
      header = this.templateSize(this.headerView, cross);
      footer = this.templateSize(this.footerView, cross);
    }
    const changed =
      along !== this.windowAlong ||
      cross !== this.windowCross ||
      header !== this.headerSize ||
      footer !== this.footerSize;
    this.windowAlong = along;
    this.windowCross = cross;
    this.headerSize = header;
    this.footerSize = footer;
    return changed;
  }

  private trackUserScroll(): void {
    const current = this.offset;
    if (Math.abs(current - this.previousOffset) < 1) return;
    if (this.scrollPhase() !== ShadowListKitCore.PHASE_IDLE) {
      this.userScrolled = true;
    }
    this.previousOffset = current;
  }

  /*
   * The template's natural size along the scroll axis at the cross size.
   */
  private templateSize(view: HTMLElement | null, cross: number): number {
    if (!view || cross <= 0) return 0;
    if (this.horizontal) {
      view.style.height = `${cross}px`;
      view.style.width = 'max-content';
      return wholePixels(view.getBoundingClientRect().width);
    }
    view.style.width = `${cross}px`;
    view.style.height = '';
    return wholePixels(view.getBoundingClientRect().height);
  }

  private layoutTemplates(): void {
    if (this.headerView) this.placeTemplate(this.headerView, 0);
    if (this.footerView) {
      this.placeTemplate(
        this.footerView,
        this.core.footerStart(this.footerSize)
      );
    }
  }

  private placeTemplate(view: HTMLElement, start: number): void {
    const transform = this.horizontal
      ? `translate(${start}px, 0)`
      : `translate(0, ${start}px)`;
    if (view.style.transform !== transform) view.style.transform = transform;
  }

  /*
   * Run the core until the window is measured and any correction landed. Everything happens
   * before the browser paints. The reader never sees an estimate or a correction.
   */
  private runPasses(): void {
    const core = this.core;
    const pass = core.pass;
    pass[ShadowListKitCore.PASS_OFFSET] = this.offset;
    pass[ShadowListKitCore.PASS_WINDOW_ALONG] = this.windowAlong;
    pass[ShadowListKitCore.PASS_WINDOW_CROSS] = this.windowCross;
    pass[ShadowListKitCore.PASS_HEADER_SIZE] = this.headerSize;
    pass[ShadowListKitCore.PASS_FOOTER_SIZE] = this.footerSize;
    pass[ShadowListKitCore.PASS_PHASE] = this.scrollPhase();
    pass[ShadowListKitCore.PASS_USER_SCROLLED] = this.userScrolled ? 1 : 0;
    pass[ShadowListKitCore.PASS_TRACKING] = this.gesture.isTracking ? 1 : 0;
    core.runPasses();
    this.userScrolled = false;
    this.needsFrame = false;
    this.applyPassResult(pass);
  }

  /*
   * The content size goes first. The offset write is then inside the scroll range.
   */
  private applyPassResult(pass: Float64Array): void {
    this.contentAlong = pass[ShadowListKitCore.PASS_OUT_CONTENT]!;
    const size = `${this.contentAlong}px`;
    if (this.horizontal) this.content.style.width = size;
    else this.content.style.height = size;
    const target = Math.round(pass[ShadowListKitCore.PASS_OUT_OFFSET]!);
    if (target !== Math.round(this.offset)) {
      this.gesture.offsetCorrected(target - this.offset);
      this.writeOffset(target);
    }
    this.bandLow = pass[ShadowListKitCore.PASS_OUT_BAND_LOW]!;
    this.bandHigh = pass[ShadowListKitCore.PASS_OUT_BAND_HIGH]!;
    this.reachedStart ||= pass[ShadowListKitCore.PASS_OUT_REACHED_START] !== 0;
    this.reachedEnd ||= pass[ShadowListKitCore.PASS_OUT_REACHED_END] !== 0;
    this.frameGeometry = pass[ShadowListKitCore.PASS_OUT_GEOMETRY]!;
    this.copyWindow(pass);
    // A correction waits for one more report on the next animation frame.
    if (pass[ShadowListKitCore.PASS_OUT_SETTLING] !== 0) this.invalidateFrame();
  }

  /*
   * Keep the window's frames on this side. Mount passes inside the band read only these.
   */
  private copyWindow(pass: Float64Array): void {
    const low = pass[ShadowListKitCore.PASS_OUT_WINDOW_LOW]!;
    const high = pass[ShadowListKitCore.PASS_OUT_WINDOW_HIGH]!;
    const geometry = pass[ShadowListKitCore.PASS_OUT_GEOMETRY]!;
    if (
      low === this.windowLow &&
      high === this.windowHigh &&
      geometry === this.windowGeometry
    ) {
      return;
    }
    this.windowLow = low;
    this.windowHigh = high;
    this.windowGeometry = geometry;
    if (low < 0) return;
    const needed = (high - low + 1) * 4;
    if (this.windowFrames.length < needed) {
      this.windowFrames = new Float64Array(needed * 2);
    }
    this.core.copyRowRects(low, high, this.windowFrames);
  }

  /*
   * Drop all sizes after a cross size change and hold the row at the viewport start, the same
   * distance into it. The core's own reset holds the first row of its last pass, which can sit
   * a few rows above. A live window resize resets on every frame and would walk back through
   * the list.
   */
  private resetKeepingPosition(): void {
    const core = this.core;
    const anchor = core.anchor(this.offset);
    core.resetKeepingPosition();
    if (anchor) core.restoreAnchor(anchor.key, anchor.offset);
    for (const cell of this.mounted.values()) cell.resetPlacement();
    this.windowLow = -1;
    this.windowHigh = -1;
    this.windowGeometry = -1;
    this.bandLow = 1;
    this.bandHigh = 0;
    this.needsFrame = true;
    this.templatesChanged = true;
    ++this.structureVersion;
  }

  // endregion

  // region Mounting

  private windowLeading(index: number): number {
    const at = (index - this.windowLow) * 4;
    return this.horizontal
      ? this.windowFrames[at]!
      : this.windowFrames[at + 1]!;
  }

  private windowExtent(index: number): number {
    const at = (index - this.windowLow) * 4;
    return this.horizontal
      ? this.windowFrames[at + 2]!
      : this.windowFrames[at + 3]!;
  }

  private overlaps(index: number, viewLow: number, viewHigh: number): boolean {
    const start = this.windowLeading(index);
    return start + this.windowExtent(index) > viewLow && start < viewHigh;
  }

  /*
   * Mount the cells of rows near the viewport and recycle the rest. Rows are matched by key,
   * which keeps the content of a cell that only moved. The range follows the core's
   * ListDriver::planMount, run here over the copied frames. A scroll frame makes no wasm call.
   * The core test list_driver_mount_plan_reaches_into_the_insets holds the rule this matches.
   *
   * The browser scrolls on its own thread and can show the next step before this pass lands.
   * Ahead of the scroll the range reaches two of the last steps, up to the measured window.
   */
  private mountCells(): void {
    if (this.windowLow < 0 || this.keys.length === 0) {
      this.unmountAll();
      return;
    }
    const offset = this.offset;
    const step = offset - this.mountedOffset;
    this.mountedOffset = offset;
    const pad = this.windowAlong * this.mountOverscan;
    const lead = Math.max(pad, Math.abs(step) * 2);
    const viewLow = offset - (step < 0 ? lead : pad);
    const viewHigh = offset + this.windowAlong + (step > 0 ? lead : pad);
    let low = -1;
    let high = -1;
    for (let index = this.windowLow; index <= this.windowHigh; ++index) {
      if (!this.overlaps(index, viewLow, viewHigh)) continue;
      if (low < 0) low = index;
      high = index;
    }
    const sticky = this.stickyPinning.activeStickyIndex(this.offset);
    if (!this.mountNeeded(low, high, sticky)) return;
    this.recordMount(low, high, sticky);
    this.mountPlan(low, high, sticky, viewLow, viewHigh);
    this.prefetchAround(low, high);
  }

  /*
   * Whether anything the last mount pass covered changed: the rows, the pinned row, the
   * geometry or the data.
   */
  private mountNeeded(low: number, high: number, sticky: number): boolean {
    return (
      low !== this.mountedLow ||
      high !== this.mountedHigh ||
      sticky !== this.mountedSticky ||
      this.windowGeometry !== this.mountedGeometry ||
      this.structureVersion !== this.mountedStructure
    );
  }

  private recordMount(low: number, high: number, sticky: number): void {
    this.mountedLow = low;
    this.mountedHigh = high;
    this.mountedSticky = sticky;
    this.mountedGeometry = this.windowGeometry;
    this.mountedStructure = this.structureVersion;
  }

  private mountPlan(
    low: number,
    high: number,
    sticky: number,
    viewLow: number,
    viewHigh: number
  ): void {
    const generation = ++this.mountGeneration;
    if (low >= 0) {
      for (let index = low; index <= high; ++index) {
        // A masonry grid can hold rows inside the index range that are out of view.
        if (
          this.numberOfColumns > 1 &&
          !this.overlaps(index, viewLow, viewHigh)
        ) {
          continue;
        }
        this.mountRow(index, generation);
      }
    }
    if (sticky >= 0 && sticky < this.keys.length) {
      this.mountRow(sticky, generation);
    }
    this.recycleStaleCells(generation);
  }

  private mountRow(index: number, generation: number): void {
    const key = this.keys[index]!;
    let cell = this.mounted.get(key);
    const appearing = !cell || cell.mountGeneration === 0;
    if (!cell) {
      cell = this.makeCell(index);
      this.mounted.set(key, cell);
    }
    cell.index = index;
    cell.mountGeneration = generation;
    this.placeCell(cell, index);
    this.applyAccessibility(cell);
    if (appearing) {
      cell.element.style.visibility = '';
      this.delegate?.willDisplayCell?.(this, cell, index);
    }
  }

  private recycleStaleCells(generation: number): void {
    for (const [key, cell] of this.mounted) {
      if (cell.mountGeneration === generation) continue;
      this.mounted.delete(key);
      this.recycleCell(cell);
    }
  }

  private unmountAll(): void {
    for (const cell of this.mounted.values()) this.recycleCell(cell);
    this.mounted.clear();
    this.mountedLow = -1;
    this.mountedHigh = -1;
  }

  /*
   * The row's frame from the window copy, or from the core for rows outside it.
   */
  private rowRect(index: number, out: Float64Array): boolean {
    if (
      this.windowLow >= 0 &&
      index >= this.windowLow &&
      index <= this.windowHigh
    ) {
      const at = (index - this.windowLow) * 4;
      out.set(this.windowFrames.subarray(at, at + 4));
      return true;
    }
    return this.core.rowRect(index, out);
  }

  /*
   * Place a row's cell at its frame. A sticky row goes in its section and sticks through CSS.
   */
  private placeCell(cell: ShadowListKitListCell, index: number): void {
    const frame = this.scratchFrame;
    if (!this.rowRect(index, frame)) return;
    if (this.stickyPinning.placeSticky(cell, index, frame)) return;
    this.placeView(cell, frame[0]!, frame[1]!, frame[2]!, frame[3]!);
  }

  /*
   * Where the rows end along the scroll axis, before the footer.
   */
  get rowsEnd(): number {
    return this.core.footerStart(this.footerSize);
  }

  /*
   * Place a cell at a frame. A cell that only moved changes only its transform.
   */
  private placeView(
    cell: ShadowListKitListCell,
    x: number,
    y: number,
    w: number,
    h: number
  ): void {
    const style = cell.element.style;
    if (cell.placedWidth !== w || cell.placedHeight !== h) {
      style.width = `${w}px`;
      style.height = `${h}px`;
      cell.placedWidth = w;
      cell.placedHeight = h;
    }
    if (cell.placedX !== x || cell.placedY !== y) {
      style.transform = `translate(${x}px, ${y}px)`;
      cell.placedX = x;
      cell.placedY = y;
    }
  }

  // endregion

  // region Prefetching

  /*
   * Tell the prefetch data source about items the measured window brought in that have no cell
   * yet, and the ones that left it unseen. One wasm call per mount pass, none on band frames.
   */
  private prefetchAround(low: number, high: number): void {
    const prefetch = this.prefetchDataSource;
    if (!prefetch) return;
    const packed = this.core.updatePrefetch(low, high);
    const prefetchCount = packed[0]!;
    const items = Array.from(packed.subarray(1, 1 + prefetchCount));
    const cancelCount = packed[1 + prefetchCount]!;
    const cancelled = Array.from(
      packed.subarray(2 + prefetchCount, 2 + prefetchCount + cancelCount)
    );
    if (items.length > 0) prefetch.prefetchItems(this, items);
    if (cancelled.length > 0) {
      prefetch.cancelPrefetchingForItems?.(this, cancelled);
    }
  }

  // endregion

  // region Measurement

  /*
   * Called by the core for every row in its window that has no size yet.
   */
  private measureItem(row: number, crossSize: number): number {
    const sizing = this.dataSource?.sizeForItem;
    if (sizing) {
      return wholePixels(sizing.call(this.dataSource, this, row, crossSize));
    }
    return this.measureCell(row, crossSize);
  }

  /*
   * Measure a row through its cell at the cross size, its size along the axis left to its
   * content. The cell is mounted right after when the row is in view, which rarely wastes the
   * configure work.
   */
  private measureCell(row: number, cross: number): number {
    const key = this.keys[row]!;
    let cell = this.mounted.get(key);
    if (!cell) {
      cell = this.makeCell(row);
      cell.mountGeneration = 0;
      this.mounted.set(key, cell);
    }
    cell.resetPlacement();
    const style = cell.element.style;
    if (this.horizontal) {
      style.height = `${cross}px`;
      style.width = 'max-content';
      return wholePixels(cell.element.getBoundingClientRect().width);
    }
    style.width = `${cross}px`;
    style.height = '';
    return wholePixels(cell.element.getBoundingClientRect().height);
  }

  // endregion

  // region Events

  private dispatchReached(): void {
    if (this.reachedStart) {
      this.reachedStart = false;
      this.delegate?.didReachStart?.(this);
    }
    if (this.reachedEnd) {
      this.reachedEnd = false;
      this.delegate?.didReachEnd?.(this);
    }
  }

  // endregion

  // region Scroll events

  /*
   * A scroll or an animated scroll ended. The core needs an idle frame to let a held correction
   * go, and an animated command lands on its row.
   */
  scrollingEnded(): void {
    this.liveCore?.land();
    this.invalidateFrame();
    this.delegate?.didEndScrolling?.(this);
  }

  gestureBegan(): void {
    this.liveCore?.cancelLanding();
  }

  /*
   * Where a scroll that rests at target moves on to.
   */
  snapTarget(target: number): number {
    return this.snapToItem ? this.core.nearestSnapOffset(target) : target;
  }

  // endregion

  // region Queries

  cellForItem(index: number): ShadowListKitListCell | null {
    const cell = this.mountedCell(index);
    return cell && cell.mountGeneration !== 0 ? cell : null;
  }

  get visibleCells(): ShadowListKitListCell[] {
    const low = this.offset;
    const high = low + this.windowAlong;
    const cells: ShadowListKitListCell[] = [];
    for (const cell of this.mounted.values()) {
      if (cell.mountGeneration === 0 || cell.index < 0) continue;
      const start = this.horizontal ? cell.placedX : cell.placedY;
      const extent = this.horizontal ? cell.placedWidth : cell.placedHeight;
      if (start + extent > low && start < high) cells.push(cell);
    }
    return cells.sort((a, b) => a.index - b.index);
  }

  /*
   * Size of the content along the scroll axis, header and footer included, in CSS pixels.
   */
  get contentSize(): number {
    return this.contentAlong;
  }

  /*
   * The items overlapping the viewport, low to high, or null before the first layout.
   */
  get visibleRange(): { low: number; high: number } | null {
    // A list without a core has not laid out. Asking would create a peer only to answer null.
    return this.liveCore?.visibleRange() ?? null;
  }

  /*
   * The item's frame in the content, the same space as the cells' frames. Estimated when the
   * row was not measured yet.
   */
  rectForItem(index: number): DOMRect | null {
    if (index < 0 || index >= this.keys.length) return null;
    const out = new Float64Array(4);
    if (!this.core.rowRect(index, out)) return null;
    return new DOMRect(out[0], out[1], out[2], out[3]);
  }

  // endregion

  // region Saved position

  /*
   * The row at the viewport start by key and how far the viewport is into it, or null before
   * the first layout.
   */
  get anchorState(): ShadowListKitAnchorState | null {
    const anchor = this.coreOrNull?.anchor(this.offset);
    if (!anchor) return this.pendingAnchor;
    return new ShadowListKitAnchorState(anchor.key, anchor.offset);
  }

  /*
   * Land the saved row the same distance in again, now or once a reload brings its key.
   */
  restoreAnchorState(state: ShadowListKitAnchorState): void {
    this.pendingAnchor = state;
    this.restorePendingAnchor();
  }

  private restorePendingAnchor(): void {
    const state = this.pendingAnchor;
    if (!state || this.keys.length === 0 || !isShadowListKitLoaded()) return;
    if (!this.core.restoreAnchor(state.key, state.offset)) return;
    this.pendingAnchor = null;
    this.stopScrolling();
    this.runCommandNow();
  }

  // endregion

  // region Accessibility

  /*
   * A cell reads as an item of the whole list, like "item 12 of 1000".
   */
  private applyAccessibility(cell: ShadowListKitListCell): void {
    const element = cell.element;
    if (!element.getAttribute('role')) element.setAttribute('role', 'listitem');
    const position = String(cell.index + 1);
    const size = String(this.itemCount);
    if (element.getAttribute('aria-posinset') !== position) {
      element.setAttribute('aria-posinset', position);
    }
    if (element.getAttribute('aria-setsize') !== size) {
      element.setAttribute('aria-setsize', size);
    }
  }

  // endregion

  // region Scroll commands

  /*
   * Bring an item into view. viewPosition is where it rests in the viewport, from 0 at the start
   * to 1 at the end. The core keeps correcting until the row lands, even across estimates.
   */
  scrollToItem(index: number, viewPosition = 0, animated = false): void {
    if (index < 0 || index >= this.keys.length) return;
    const target = () =>
      this.core.animatedTargetOffset(
        index,
        viewPosition,
        this.windowAlong,
        this.maxOffset
      );
    if (animated && !Number.isNaN(target())) {
      // Animate to the estimate, then let the core land exactly when the animation ends.
      this.animateCommand(
        target,
        ShadowListKitCore.LANDING_INDEX,
        index,
        viewPosition
      );
      return;
    }
    this.stopScrolling();
    this.core.scrollToIndex(index, viewPosition);
    this.runCommandNow();
  }

  scrollToStart(animated = false): void {
    if (animated) {
      // The header shows at the very start, which landing on row 0 would scroll past.
      this.animateCommand(() => 0, ShadowListKitCore.LANDING_START);
      return;
    }
    this.stopScrolling();
    this.core.scrollToStart();
    this.runCommandNow();
  }

  scrollToEnd(animated = false): void {
    if (animated) {
      this.animateCommand(() => this.maxOffset, ShadowListKitCore.LANDING_END);
      return;
    }
    this.stopScrolling();
    this.core.scrollToEnd();
    this.runCommandNow();
  }

  private animateCommand(
    target: () => number,
    landing: number,
    index = 0,
    viewPosition = 0
  ): void {
    this.core.setLanding(landing, index, viewPosition);
    this.gesture.animateTo(target);
  }

  /*
   * Stop any animated command still on its way. Otherwise it keeps writing the offset over a
   * command that runs now.
   */
  private stopScrolling(): void {
    this.gesture.stop();
    this.core.cancelLanding();
  }

  runCommandNow(): void {
    this.needsFrame = true;
    if (this.isConnected) this.layoutPass();
  }

  // endregion
}

if (
  typeof customElements !== 'undefined' &&
  !customElements.get(ShadowListKitListView.TAG_NAME)
) {
  customElements.define(ShadowListKitListView.TAG_NAME, ShadowListKitListView);
}
