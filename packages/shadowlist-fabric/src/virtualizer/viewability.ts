import type { MountedRange } from './mountedRange';
import type {
  ViewabilityConfig,
  ViewableItemsChangedInfo,
  ViewToken,
} from '../types';

/*
 * The viewable range from native's low and high, or null when nothing is viewable.
 */
export function viewableRange(low: number, high: number): MountedRange | null {
  if (low === -1 || high === -1) return null;
  return { low, high };
}

/*
 * The section header the sticky overlay shows for a range starting at rangeLow: the last
 * sticky index at or above it, or -1. Indices are ascending.
 */
export function activeStickyIndexFor(
  stickyIndices: ReadonlyArray<number> | undefined,
  rangeLow: number
): number {
  let active = -1;
  if (!stickyIndices) return active;
  for (const stickyIndex of stickyIndices) {
    if (stickyIndex <= rangeLow) active = stickyIndex;
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
 * One viewability config and its callback. Tokens are
 * compared by key, and only a change calls back. With waitForInteraction nothing is viewable
 * before recordInteraction. With minimumViewTime the rows must stay viewable that long. Each
 * change starts the wait again, and the rows viewable when it ends are reported.
 */
export class ViewabilityTracker<ItemT> {
  private reported: ViewToken<ItemT>[] = [];
  private range: MountedRange | null = null;
  private interacted = false;
  private timer: unknown = null;

  constructor(
    public config: ViewabilityConfig,
    public onViewableItemsChanged:
      | ((info: ViewableItemsChangedInfo<ItemT>) => void)
      | null
      | undefined,
    private readonly timers: ViewabilityTimers = DEFAULT_TIMERS
  ) {}

  recordInteraction(
    build: (low: number, high: number) => ViewToken<ItemT>[]
  ): void {
    if (this.interacted) return;
    this.interacted = true;
    this.update(this.range, build);
  }

  /*
   * The viewable range changed, or the rows in it did.
   */
  update(
    range: MountedRange | null,
    build: (low: number, high: number) => ViewToken<ItemT>[]
  ): void {
    this.range = range;
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

  private emit(build: (low: number, high: number) => ViewToken<ItemT>[]): void {
    const viewableItems = this.range
      ? build(this.range.low, this.range.high)
      : [];
    const currentKeys = new Set(viewableItems.map((token) => token.key));
    const previousKeys = new Set(this.reported.map((token) => token.key));
    const changed: ViewToken<ItemT>[] = [
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
