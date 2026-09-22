import { SHADOWLIST_OVERSCAN } from './helpers';

/*
 * The mounted range, low to high. Mounts overscanRows extra rows on each side of the screen
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
  overscanRows: number = SHADOWLIST_OVERSCAN,
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
      low: Math.max(0, target - overscanRows - before),
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
 * Whether a new containerOffsetIndex should rebuild the mounted rows around it, instead of
 * where native last reported. A negative value means no target, and must not pull a reader
 * who scrolled away back to the start.
 */
export function shouldReseedFromOffsetIndex(
  previousOffsetIndex: number,
  nextOffsetIndex: number
): boolean {
  return nextOffsetIndex !== previousOffsetIndex && nextOffsetIndex >= 0;
}
