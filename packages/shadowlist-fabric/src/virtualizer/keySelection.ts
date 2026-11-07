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
 * A terminal has no other way to move far, so a list there also takes the paging keys.
 */
export const PAGING_SELECTION_KEYS = [
  ...SELECTION_KEYS,
  { key: 'PageUp' },
  { key: 'PageDown' },
  { key: 'Home' },
  { key: 'End' },
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
 * The row a paging key selects: a screen of rows from index for PageUp and PageDown, an end of the
 * list for Home and End, moving on toward the far end past rows that can't be selected. Returns -1
 * for a key that is none of those or when no row that way can be selected.
 */
export function pagedSelectableIndex(
  key: string,
  index: number,
  page: number,
  count: number,
  canSelect: (index: number) => boolean
): number {
  const from = Math.max(index, 0);
  const lastRow = count - 1;
  switch (key) {
    case 'PageDown':
      return nextSelectableIndex(
        Math.min(from + page, lastRow) - 1,
        1,
        count,
        canSelect
      );
    case 'End':
      return nextSelectableIndex(lastRow + 1, -1, count, canSelect);
    case 'PageUp':
      return nextSelectableIndex(
        Math.max(from - page, 0) + 1,
        -1,
        count,
        canSelect
      );
    case 'Home':
      return nextSelectableIndex(-1, 1, count, canSelect);
    default:
      return -1;
  }
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
