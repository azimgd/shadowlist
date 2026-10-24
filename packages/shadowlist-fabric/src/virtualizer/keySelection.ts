import type { MountedRange } from './useMountedRange';

/*
 * The keys a list with enableSelectionOnKeyPress takes from the keyboard.
 */
export const SELECTION_KEYS = [
  { key: 'ArrowUp' },
  { key: 'ArrowDown' },
  { key: 'Enter' },
  { key: ' ' },
];

/*
 * The next row an arrow key selects: one step from index, skipping rows that can't be
 * selected. From no selection (-1) Down starts at the first row and Up at the last. Returns -1
 * when no row that way can be selected.
 */
export function nextSelectableIndex(
  index: number,
  step: 1 | -1,
  count: number,
  canSelect: (index: number) => boolean
): number {
  let next = index < 0 ? (step > 0 ? 0 : count - 1) : index + step;
  while (next >= 0 && next < count) {
    if (canSelect(next)) return next;
    next += step;
  }
  return -1;
}

/*
 * Whether a row needs scrolling to come fully into view, given the visible rows. The first and
 * last of those may be cut off.
 */
export function needsScrollIntoView(
  index: number,
  visible: MountedRange | null
): boolean {
  return visible === null || index <= visible.low || index >= visible.high;
}
