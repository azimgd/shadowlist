import type { MountedRange } from './mountedRange';

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
  stickyHeaderIndices: ReadonlyArray<number> | undefined,
  windowLow: number
): number {
  let active = -1;
  if (!stickyHeaderIndices) return active;
  for (const stickyIndex of stickyHeaderIndices) {
    if (stickyIndex <= windowLow) active = stickyIndex;
    else break;
  }
  return active;
}
