import { SHADOWLIST_OVERSCAN } from './helpers';

/*
 * Mounted range [low, high]. Mounts `overscanRows` extra rows on each side of the visible
 * window and only re-renders when the window leaves the mounted range.
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
   * With an explicit initial target, seed the range around it (avoids a blank flash
   * at the target). An explicit target overrides the inverted bottom anchor.
   *
   * viewPosition decides which side of the target the reader ends up looking at: a
   * start-aligned jump fills the screen with the rows after it, a centred or end-aligned
   * one with the rows before it. Split the band by the same fraction.
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

export function rangeToIndices(range: MountedRange): number[] {
  if (range.low < 0 || range.high < 0 || range.low > range.high) return [];
  const indices: number[] = [];
  for (let index = range.low; index <= range.high; index++) indices.push(index);
  return indices;
}

/*
 * Whether a change of containerOffsetIndex should rebuild the mounted window around the
 * new target, rather than leave it where the last native report put it. A negative value
 * is "no target" (see initialMountedRange) and must not drag a reader who has scrolled
 * away back to the seed.
 */
export function shouldReseedFromOffsetIndex(
  previousOffsetIndex: number,
  nextOffsetIndex: number
): boolean {
  return nextOffsetIndex !== previousOffsetIndex && nextOffsetIndex >= 0;
}
