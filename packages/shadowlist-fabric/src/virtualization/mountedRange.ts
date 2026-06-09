/*
 * Mounted range [low, high]. Mounts an overscan band around the visible range and
 * only re-renders when the visible range leaves the mounted range.
 */
export interface MountedRange {
  low: number;
  high: number;
}

export const SHADOWLIST_OVERSCAN = 4;

export const initialMountedRange = (
  size: number,
  initial: number,
  inverted: boolean,
  offsetIndex: number
): MountedRange => {
  if (size <= 0) return { low: -1, high: -1 };
  /*
   * With an explicit initial target, seed the range around it (avoids a blank flash
   * at the target). An explicit target overrides the inverted bottom anchor.
   */
  if (offsetIndex >= 0) {
    const target = Math.min(offsetIndex, size - 1);
    return {
      low: Math.max(0, target - SHADOWLIST_OVERSCAN),
      high: Math.min(size - 1, target + initial),
    };
  }
  if (inverted) {
    return { low: Math.max(0, size - initial), high: size - 1 };
  }
  return { low: 0, high: Math.min(initial, size - 1) };
};

export const rangeToIndices = (range: MountedRange): number[] => {
  if (range.low < 0 || range.high < 0 || range.low > range.high) return [];
  const indices: number[] = [];
  for (let index = range.low; index <= range.high; index++) indices.push(index);
  return indices;
};
