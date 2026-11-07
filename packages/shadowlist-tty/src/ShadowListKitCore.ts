import type { ShadowListStoreModule } from '../../shadowlist-fabric/src/virtualizer/nativeStore';

/*
 * TypeScript side of cpp/ShadowListKitCoreNapi.cpp: the addon, and one list's core. Every call is
 * a thin hop into the core's ListDriver. The pass array is reused, so a layout allocates nothing
 * here.
 */

/*
 * What the addon's driver object holds. The first argument of every function is the driver.
 */
interface DriverFunctions {
  createDriver(): unknown;
  setSettings(
    driver: unknown,
    estimatedRowSize: number,
    overscan: number,
    startReachedThreshold: number,
    endReachedThreshold: number,
    numberOfColumns: number,
    inverted: boolean,
    followAppends: boolean,
    invertedFollowBand: number,
    horizontal: boolean,
    snapToItem: boolean,
    snapAlignment: number
  ): void;
  adoptKeys(driver: unknown, storeId: number, version: number): boolean;
  setStickyIndices(driver: unknown, sticky: ReadonlyArray<number>): void;
  runPasses(
    driver: unknown,
    pass: Float64Array,
    measure: (index: number, key: string) => number
  ): void;
  markRemeasure(driver: unknown, index: number): void;
  resetKeepingPosition(driver: unknown): void;
  getRowRect(driver: unknown, index: number, out: Float64Array): void;
  getLeadingAt(driver: unknown, index: number): number;
  getExtentAt(driver: unknown, index: number): number;
  getRowCount(driver: unknown): number;
  getKeyAt(driver: unknown, index: number): string;
  indexOfKey(driver: unknown, key: string): number;
  getVisibleRange(driver: unknown, out: Float64Array): boolean;
  activeStickyIndex(driver: unknown, offset: number): number;
  stickyLeading(driver: unknown, active: number, offset: number): number;
  stickyTranslations(
    driver: unknown,
    input: Float64Array,
    out: Float64Array
  ): void;
  scrollToRow(
    driver: unknown,
    index: number,
    viewPosition: number,
    viewOffset: number
  ): void;
  scrollToEnd(driver: unknown): void;
  nearestSnapOffset(driver: unknown, target: number): number;
  getAnchorState(
    driver: unknown,
    offset: number
  ): { key: string; offset: number } | null;
  updatePrefetch(
    driver: unknown,
    low: number,
    high: number
  ): { prefetch: number[]; cancel: number[] } | null;
  updateViewability(
    driver: unknown,
    config: number,
    minimumViewTime: number,
    waitForInteraction: boolean,
    interacted: boolean,
    viewable: ReadonlyArray<string>,
    now: number
  ): ViewabilityUpdate;
}

export interface ViewabilityUpdate {
  waiting: boolean;
  added: string[];
  removed: string[];
}

interface Addon {
  store: ShadowListStoreModule;
  driver: DriverFunctions;
}

let addon: Addon | null = null;

function loadAddon(): Addon {
  if (addon === null) {
    const location =
      process.env.SHADOWLIST_TTY_ADDON ??
      decodeURIComponent(
        new URL('../build/shadowlist_tty.node', import.meta.url).pathname
      );
    const loaded = { exports: {} };
    process.dlopen(loaded, location);
    addon = loaded.exports as Addon;
  }
  return addon;
}

/*
 * The ShadowListStore module: the addon's functions, loaded on the first call so that importing a
 * list does not need the addon's path yet.
 */
export function shadowListStoreModule(): ShadowListStoreModule {
  const functions = [
    'createStore',
    'setKeys',
    'spliceKeys',
    'setSections',
    'resetTree',
    'expandTree',
    'collapseTree',
    'indexOfKey',
    'setSizeSpecs',
    'setSelection',
    'selectKey',
    'selectOnlyKey',
    'deselectKey',
    'retainSelection',
    'selectedIndices',
  ] as const;
  const module: Record<string, unknown> = {};
  for (const name of functions) {
    module[name] = (...args: never[]) =>
      (loadAddon().store[name] as (...values: never[]) => unknown)(...args);
  }
  return module as unknown as ShadowListStoreModule;
}

export const PHASE_IDLE = 0;
export const PHASE_DRAGGING = 1;

/*
 * Slots of the pass array, PassSlot in the addon. The first block is what the view reports, the
 * second what it applies.
 */
const PASS_OFFSET = 0;
const PASS_WINDOW_ALONG = 1;
const PASS_WINDOW_CROSS = 2;
const PASS_HEADER_SIZE = 3;
const PASS_FOOTER_SIZE = 4;
const PASS_PHASE = 5;
const PASS_USER_SCROLLED = 6;
const PASS_OUT_OFFSET = 7;
const PASS_OUT_CONTENT = 8;
const PASS_OUT_BAND_LOW = 9;
const PASS_OUT_BAND_HIGH = 10;
const PASS_OUT_SETTLING = 11;
const PASS_OUT_REACHED_START = 12;
const PASS_OUT_REACHED_END = 13;
const PASS_OUT_FOOTER_START = 14;
const PASS_SLOTS = 15;

const STICKY_SLOTS = 13;

export interface ListSettings {
  overscan: number;
  startReachedThreshold: number;
  endReachedThreshold: number;
  numberOfColumns: number;
  inverted: boolean;
  followAppends: boolean;
  horizontal: boolean;
  snapToItem: boolean;
  snapAlignment: number;
}

export interface PassInput {
  offset: number;
  windowAlong: number;
  windowCross: number;
  headerSize: number;
  footerSize: number;
  phase: number;
  userScrolled: boolean;
}

export interface PassResult {
  offset: number;
  content: number;
  bandLow: number;
  bandHigh: number;
  settling: boolean;
  reachedStart: boolean;
  reachedEnd: boolean;
  footerStart: number;
}

export interface StickyInput {
  offset: number;
  windowSize: number;
  contentSize: number;
  hasHeader: boolean;
  headerSize: number;
  stickyHeader: boolean;
  autoHideHeader: boolean;
  hasFooter: boolean;
  footerSize: number;
  footerStart: number;
  stickyFooter: boolean;
  autoHideFooter: boolean;
  accumulate: boolean;
}

export interface RowRect {
  x: number;
  y: number;
  width: number;
  height: number;
}

/*
 * A row is one cell until it mounts, and the band around an inverted list's end is one cell: a
 * wheel notch is three cells, and leaving the end takes less than that.
 */
const ESTIMATED_ROW_SIZE = 1;
const INVERTED_FOLLOW_BAND = 1;

export class ShadowListKitCore {
  private readonly functions: DriverFunctions = loadAddon().driver;
  private readonly driver: unknown = this.functions.createDriver();
  private readonly pass = new Float64Array(PASS_SLOTS);
  private readonly out = new Float64Array(4);
  private readonly stickyIn = new Float64Array(STICKY_SLOTS);

  setSettings(settings: ListSettings): void {
    this.functions.setSettings(
      this.driver,
      ESTIMATED_ROW_SIZE,
      settings.overscan,
      settings.startReachedThreshold,
      settings.endReachedThreshold,
      settings.numberOfColumns,
      settings.inverted,
      settings.followAppends,
      INVERTED_FOLLOW_BAND,
      settings.horizontal,
      settings.snapToItem,
      settings.snapAlignment
    );
  }

  adoptKeys(storeId: number, version: number): boolean {
    return this.functions.adoptKeys(this.driver, storeId, version);
  }

  setStickyIndices(sticky: ReadonlyArray<number>): void {
    this.functions.setStickyIndices(this.driver, sticky);
  }

  runPasses(
    input: PassInput,
    measure: (index: number, key: string) => number
  ): PassResult {
    const { pass } = this;
    pass[PASS_OFFSET] = input.offset;
    pass[PASS_WINDOW_ALONG] = input.windowAlong;
    pass[PASS_WINDOW_CROSS] = input.windowCross;
    pass[PASS_HEADER_SIZE] = input.headerSize;
    pass[PASS_FOOTER_SIZE] = input.footerSize;
    pass[PASS_PHASE] = input.phase;
    pass[PASS_USER_SCROLLED] = input.userScrolled ? 1 : 0;
    this.functions.runPasses(this.driver, pass, measure);
    return {
      offset: pass[PASS_OUT_OFFSET]!,
      content: pass[PASS_OUT_CONTENT]!,
      bandLow: pass[PASS_OUT_BAND_LOW]!,
      bandHigh: pass[PASS_OUT_BAND_HIGH]!,
      settling: pass[PASS_OUT_SETTLING] !== 0,
      reachedStart: pass[PASS_OUT_REACHED_START] !== 0,
      reachedEnd: pass[PASS_OUT_REACHED_END] !== 0,
      footerStart: pass[PASS_OUT_FOOTER_START]!,
    };
  }

  markRemeasure(index: number): void {
    this.functions.markRemeasure(this.driver, index);
  }

  resetKeepingPosition(): void {
    this.functions.resetKeepingPosition(this.driver);
  }

  getRowRect(index: number): RowRect {
    this.functions.getRowRect(this.driver, index, this.out);
    const { out } = this;
    return { x: out[0]!, y: out[1]!, width: out[2]!, height: out[3]! };
  }

  getLeadingAt(index: number): number {
    return this.functions.getLeadingAt(this.driver, index);
  }

  getExtentAt(index: number): number {
    return this.functions.getExtentAt(this.driver, index);
  }

  getRowCount(): number {
    return this.functions.getRowCount(this.driver);
  }

  getKeyAt(index: number): string {
    return this.functions.getKeyAt(this.driver, index);
  }

  indexOfKey(key: string): number {
    return this.functions.indexOfKey(this.driver, key);
  }

  getVisibleRange(): { low: number; high: number } | null {
    return this.functions.getVisibleRange(this.driver, this.out)
      ? { low: this.out[0]!, high: this.out[1]! }
      : null;
  }

  activeStickyIndex(offset: number): number {
    return this.functions.activeStickyIndex(this.driver, offset);
  }

  stickyLeading(active: number, offset: number): number {
    return this.functions.stickyLeading(this.driver, active, offset);
  }

  stickyTranslations(input: StickyInput): { header: number; footer: number } {
    const { stickyIn, out } = this;
    stickyIn[0] = input.offset;
    stickyIn[1] = input.windowSize;
    stickyIn[2] = input.contentSize;
    stickyIn[3] = input.hasHeader ? 1 : 0;
    stickyIn[4] = input.headerSize;
    stickyIn[5] = input.stickyHeader ? 1 : 0;
    stickyIn[6] = input.autoHideHeader ? 1 : 0;
    stickyIn[7] = input.hasFooter ? 1 : 0;
    stickyIn[8] = input.footerSize;
    stickyIn[9] = input.footerStart;
    stickyIn[10] = input.stickyFooter ? 1 : 0;
    stickyIn[11] = input.autoHideFooter ? 1 : 0;
    stickyIn[12] = input.accumulate ? 1 : 0;
    this.functions.stickyTranslations(this.driver, stickyIn, out);
    return { header: out[0]!, footer: out[1]! };
  }

  scrollToRow(index: number, viewPosition: number, viewOffset: number): void {
    this.functions.scrollToRow(this.driver, index, viewPosition, viewOffset);
  }

  scrollToEnd(): void {
    this.functions.scrollToEnd(this.driver);
  }

  nearestSnapOffset(target: number): number {
    return this.functions.nearestSnapOffset(this.driver, target);
  }

  getAnchorState(offset: number): { key: string; offset: number } | null {
    return this.functions.getAnchorState(this.driver, offset);
  }

  updatePrefetch(
    low: number,
    high: number
  ): { prefetch: number[]; cancel: number[] } | null {
    return this.functions.updatePrefetch(this.driver, low, high);
  }

  updateViewability(
    config: number,
    minimumViewTime: number,
    waitForInteraction: boolean,
    interacted: boolean,
    viewable: ReadonlyArray<string>,
    now: number
  ): ViewabilityUpdate {
    return this.functions.updateViewability(
      this.driver,
      config,
      minimumViewTime,
      waitForInteraction,
      interacted,
      viewable,
      now
    );
  }
}
