import type { ViewToken } from '../types';

export interface ViewableItemsDiff<ElementT> {
  /* Items currently viewable, ascending by index. */
  viewableItems: ViewToken<ElementT>[];
  /* Items that entered (isViewable: true) followed by items that left (false). */
  changed: ViewToken<ElementT>[];
}

/*
 * Build the viewable-token set for a native viewable window and diff it against the
 * previous set by key. An uninitialized window (-1) resolves to an empty set, so
 * everything previously viewable reports as left. Indices with no backing item
 * (data races a native emit) are skipped.
 */
export const diffViewableItems = <ElementT>(
  data: ReadonlyArray<ElementT>,
  viewableStartIndex: number,
  viewableEndIndex: number,
  prevViewable: ReadonlyArray<ViewToken<ElementT>>,
  keyExtractor: (item: ElementT, index: number) => string
): ViewableItemsDiff<ElementT> => {
  const viewableItems: ViewToken<ElementT>[] = [];
  if (viewableStartIndex !== -1 && viewableEndIndex !== -1) {
    // Normalise to an ascending range (inverted lists report higher index first).
    const lo = Math.min(viewableStartIndex, viewableEndIndex);
    const hi = Math.max(viewableStartIndex, viewableEndIndex);
    for (let index = lo; index <= hi; index++) {
      const item = data[index];
      if (!item) continue;
      viewableItems.push({
        item,
        index,
        key: keyExtractor(item, index),
        isViewable: true,
      });
    }
  }

  const currentKeys = new Set(viewableItems.map((token) => token.key));
  const prevKeys = new Set(prevViewable.map((token) => token.key));

  const changed: ViewToken<ElementT>[] = [
    ...viewableItems.filter((token) => !prevKeys.has(token.key)),
    ...prevViewable
      .filter((token) => !currentKeys.has(token.key))
      .map((token) => ({ ...token, isViewable: false })),
  ];

  return { viewableItems, changed };
};

/*
 * Flat index of the section header pinned for a window starting at `windowLow`: the
 * last sticky index at or above the top of the viewport, or -1 while scrolled above
 * the first section header. `stickyHeaderIndices` must be ascending.
 */
export const resolveActiveStickyIndex = (
  stickyHeaderIndices: ReadonlyArray<number> | undefined,
  windowLow: number
): number => {
  if (!stickyHeaderIndices || stickyHeaderIndices.length === 0) return -1;
  let active = -1;
  for (const stickyIndex of stickyHeaderIndices) {
    if (stickyIndex <= windowLow) active = stickyIndex;
    else break;
  }
  return active;
};
