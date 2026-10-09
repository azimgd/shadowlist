import type { MountedRange } from './mountedRange';
import type {
  ViewabilityConfig,
  ViewableItemsChangedInfo,
  ViewToken,
} from '../types';

/*
 * The viewable window from native's start and end, low to high, or null when nothing is
 * viewable. Inverted lists report start after end.
 */
export function viewableWindow(
  startIndex: number,
  endIndex: number
): MountedRange | null {
  if (startIndex === -1 || endIndex === -1) return null;
  return {
    low: Math.min(startIndex, endIndex),
    high: Math.max(startIndex, endIndex),
  };
}

/*
 * The section header the sticky overlay shows for a window starting at windowLow: the last
 * sticky index at or above it, or -1. Indices are ascending.
 */
export function activeStickyIndexFor(
  stickyIndices: ReadonlyArray<number> | undefined,
  windowLow: number
): number {
  let active = -1;
  if (!stickyIndices) return active;
  for (const stickyIndex of stickyIndices) {
    if (stickyIndex <= windowLow) active = stickyIndex;
    else break;
  }
  return active;
}

/*
 * The rules native reports viewable rows for, two numbers each: the threshold from 0 to 1,
 * then 1 for viewport coverage or 0 for the row's own share. The first rule is any overlap,
 * which picks the pinned section header. One rule follows for each config.
 */
export function viewableRulesFor(
  configs: ReadonlyArray<ViewabilityConfig>
): number[] {
  const rules = [0, 0];
  for (const config of configs) {
    const coverage = config.viewAreaCoveragePercentThreshold != null;
    const percent = coverage
      ? config.viewAreaCoveragePercentThreshold!
      : (config.itemVisiblePercentThreshold ?? 0);
    rules.push(Math.min(100, Math.max(0, percent)) / 100, coverage ? 1 : 0);
  }
  return rules;
}

export interface ViewabilityTimers {
  setTimeout: (callback: () => void, delay: number) => unknown;
  clearTimeout: (handle: unknown) => void;
}

const DEFAULT_TIMERS: ViewabilityTimers = {
  setTimeout: (callback, delay) => setTimeout(callback, delay),
  clearTimeout: (handle) =>
    clearTimeout(handle as ReturnType<typeof setTimeout>),
};

/*
 * One viewability config and its callback, like FlatList's ViewabilityHelper. Tokens are
 * compared by key, and only a change calls back. With waitForInteraction nothing is viewable
 * before recordInteraction. With minimumViewTime the rows must stay viewable that long. Each
 * change starts the wait again, and the rows viewable when it ends are reported.
 */
export class ViewabilityTracker<ElementT> {
  private reported: ViewToken<ElementT>[] = [];
  private window: MountedRange | null = null;
  private interacted = false;
  private timer: unknown = null;

  constructor(
    public config: ViewabilityConfig,
    public onViewableItemsChanged:
      | ((info: ViewableItemsChangedInfo<ElementT>) => void)
      | null
      | undefined,
    private readonly timers: ViewabilityTimers = DEFAULT_TIMERS
  ) {}

  recordInteraction(
    build: (low: number, high: number) => ViewToken<ElementT>[]
  ): void {
    if (this.interacted) return;
    this.interacted = true;
    this.update(this.window, build);
  }

  /*
   * The viewable window changed, or the rows in it did.
   */
  update(
    window: MountedRange | null,
    build: (low: number, high: number) => ViewToken<ElementT>[]
  ): void {
    this.window = window;
    if (this.config.waitForInteraction && !this.interacted) return;
    const wait = this.config.minimumViewTime ?? 0;
    if (wait <= 0) {
      this.emit(build);
      return;
    }
    if (this.timer !== null) this.timers.clearTimeout(this.timer);
    this.timer = this.timers.setTimeout(() => {
      this.timer = null;
      this.emit(build);
    }, wait);
  }

  dispose(): void {
    if (this.timer !== null) this.timers.clearTimeout(this.timer);
    this.timer = null;
  }

  private emit(
    build: (low: number, high: number) => ViewToken<ElementT>[]
  ): void {
    const viewableItems = this.window
      ? build(this.window.low, this.window.high)
      : [];
    const currentKeys = new Set(viewableItems.map((token) => token.key));
    const previousKeys = new Set(this.reported.map((token) => token.key));
    const changed: ViewToken<ElementT>[] = [
      ...viewableItems.filter((token) => !previousKeys.has(token.key)),
      ...this.reported
        .filter((token) => !currentKeys.has(token.key))
        .map((token) => ({ ...token, isViewable: false })),
    ];
    this.reported = viewableItems;
    if (changed.length > 0) {
      this.onViewableItemsChanged?.({ viewableItems, changed });
    }
  }
}
