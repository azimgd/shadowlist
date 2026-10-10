import { SHADOWLIST_OVERSCAN } from './helpers';

/*
 * The mounted range, low to high. Mounts mountOverscanRows extra rows on each side of the screen
 * and only re-renders when the screen leaves that range.
 */
export interface MountedRange {
  low: number;
  high: number;
}

export function initialMountedRange(
  size: number,
  initial: number,
  inverted: boolean,
  offsetIndex: number,
  mountOverscanRows: number = SHADOWLIST_OVERSCAN,
  viewPosition: number = 0
): MountedRange {
  if (size <= 0) return { low: -1, high: -1 };
  /*
   * With a starting target, mount around it so it doesn't flash blank. It wins over the
   * inverted list's bottom start.
   * viewPosition decides which side of the target is on screen. Aligned to the start the
   * rows after it fill the screen, centered or at the end the rows before it. Split the
   * mounted rows the same way.
   */
  if (offsetIndex >= 0) {
    const target = Math.min(offsetIndex, size - 1);
    const before = Math.round(viewPosition * initial);
    return {
      low: Math.max(0, target - mountOverscanRows - before),
      high: Math.min(size - 1, target + (initial - before)),
    };
  }
  if (inverted) {
    return { low: Math.max(0, size - initial), high: size - 1 };
  }
  return { low: 0, high: Math.min(initial, size - 1) };
}

/*
 * Indices of both ranges, sorted and without duplicates. While a scroll command is on its
 * way, mount the rows around its target and keep the rows still on screen. Native jumps a
 * frame or more later, and unmounting the visible rows early would show a blank screen.
 */
export function unionRangeIndices(
  first: MountedRange,
  second: MountedRange
): number[] {
  const firstIndices = rangeToIndices(first);
  const secondIndices = rangeToIndices(second);
  if (firstIndices.length === 0) return secondIndices;
  if (secondIndices.length === 0) return firstIndices;
  const [lower, upper] =
    first.low <= second.low ? [first, second] : [second, first];
  if (upper.low <= lower.high + 1) {
    return rangeToIndices({
      low: lower.low,
      high: Math.max(lower.high, upper.high),
    });
  }
  return [...rangeToIndices(lower), ...rangeToIndices(upper)];
}

export function rangeToIndices(range: MountedRange): number[] {
  if (range.low < 0 || range.high < 0 || range.low > range.high) return [];
  const indices: number[] = [];
  for (let index = range.low; index <= range.high; index++) indices.push(index);
  return indices;
}

/*
 * Whether a new scrollIndex should rebuild the mounted rows around it, instead of
 * where native last reported. A negative value means no target, and must not pull a reader
 * who scrolled away back to the start.
 */
export function shouldReseedFromOffsetIndex(
  previousOffsetIndex: number,
  nextOffsetIndex: number
): boolean {
  return nextOffsetIndex !== previousOffsetIndex && nextOffsetIndex >= 0;
}

/*
 * One step from the mounted range toward the target: measured rows mount right
 * away, the overscan beyond them grows by at most step rows per end. Mounting a whole
 * overscan pad at once puts ten or more new rows, each a fresh subtree, into one commit and
 * one long frame on both threads. Shrinking is never paced. A target that doesn't overlap
 * the mounted rows, like after a jump, grows from the measured range instead.
 */
export function stepMountedRange(
  current: MountedRange,
  target: MountedRange,
  measured: MountedRange,
  step: number
): MountedRange {
  const disjoint =
    current.low < 0 ||
    current.high < 0 ||
    target.low > current.high ||
    target.high < current.low;
  const base = disjoint ? measured : current;
  const low =
    target.low >= base.low
      ? target.low
      : Math.max(target.low, Math.min(base.low - step, measured.low));
  const high =
    target.high <= base.high
      ? target.high
      : Math.min(target.high, Math.max(base.high + step, measured.high));
  return { low, high };
}

/*
 * Overscan rows added per step, at least a quarter of the measured range so a fling over short rows
 * still builds a leading pad within a few frames.
 */
export function mountStepForRange(
  measured: MountedRange,
  minimumStep: number
): number {
  const measuredRows = measured.high - measured.low + 1;
  return Math.max(minimumStep, Math.ceil(measuredRows / 4));
}

/*
 * How many appended rows an inverted list at its end mounts on top of its range. A bigger
 * burst, like a reconnect syncing hundreds of messages, moves the range to the tail instead.
 */
export const MAX_FOLLOWED_APPEND = 50;

/*
 * The mounted range from its edge rows' current indices. A range at an edge of the data grows
 * by up to the leading pad to take rows added past it, and a followed tail takes every append.
 */
export function grownMountedRange(
  lowIndex: number,
  highIndex: number,
  lowAtStart: boolean,
  highAtEnd: boolean,
  size: number,
  mountOverscanRowsLeading: number,
  followTail: boolean
): MountedRange {
  const low = Math.min(lowIndex, highIndex);
  const high = Math.max(lowIndex, highIndex);
  const grownLow = lowAtStart
    ? Math.max(0, low - mountOverscanRowsLeading)
    : low;
  if (followTail && highAtEnd) {
    const tailHigh = size - 1;
    const tailLow = tailHigh - (high - low) - MAX_FOLLOWED_APPEND;
    return { low: Math.max(grownLow, tailLow), high: tailHigh };
  }
  return {
    low: grownLow,
    high: highAtEnd
      ? Math.min(size - 1, high + mountOverscanRowsLeading)
      : high,
  };
}

/*
 * Where the mounted range should end up for a measured range, with the leading pad on the side
 * the measured range moved toward.
 */
export function visibleTargetRange(
  measured: MountedRange,
  previousMeasured: MountedRange | null,
  size: number,
  mountOverscanRows: number,
  mountOverscanRowsLeading: number
): MountedRange {
  const movingForward = previousMeasured
    ? measured.low > previousMeasured.low
    : false;
  const movingBackward = previousMeasured
    ? measured.low < previousMeasured.low
    : false;
  const lowPad = movingBackward ? mountOverscanRowsLeading : mountOverscanRows;
  const highPad = movingForward ? mountOverscanRowsLeading : mountOverscanRows;
  return {
    low: Math.max(0, measured.low - lowPad),
    high: Math.min(size - 1, measured.high + highPad),
  };
}

/*
 * The next step and its target for a visible rows report, or null to keep the range. The
 * first report always steps, which trims the initial range guessed before layout. A range
 * that holds the screen is kept unless rows inserted inside it made it far bigger than the
 * target, like a tree expanding every folder. Then it shrinks to the target.
 */
export function reportedMountedRange(
  current: MountedRange,
  measured: MountedRange,
  previousMeasured: MountedRange | null,
  firstReport: boolean,
  size: number,
  mountOverscanRows: number,
  mountOverscanRowsLeading: number,
  minimumStep: number
): { range: MountedRange; target: MountedRange } | null {
  const holdsMeasured =
    current.low >= 0 &&
    measured.low >= current.low &&
    measured.high <= current.high;
  const target = visibleTargetRange(
    measured,
    previousMeasured,
    size,
    mountOverscanRows,
    mountOverscanRowsLeading
  );
  const targetCount = target.high - target.low + 1;
  const excess = current.high - current.low + 1 - targetCount;
  const oversized = excess > Math.max(targetCount, MAX_FOLLOWED_APPEND);
  if (holdsMeasured && !firstReport && !oversized) return null;
  const range = stepMountedRange(
    current,
    target,
    measured,
    mountStepForRange(measured, minimumStep)
  );
  return { range, target };
}
