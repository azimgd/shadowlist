export interface ViewableRange {
  low: number;
  high: number;
}

/*
 * The index range in an `onViewableItemsChanged` payload, or `undefined` when nothing is
 * viewable. The tokens don't need to be sorted.
 *
 * For example:
 *
 *   onViewableItemsChanged={({ viewableItems }) => {
 *     const range = getViewableRange(viewableItems);
 *     analytics.track('rows_seen', range);
 *   }}
 */
export function getViewableRange(
  viewableItems: ReadonlyArray<{ index: number }>
): ViewableRange | undefined {
  if (viewableItems.length === 0) return undefined;
  let low = Infinity;
  let high = -Infinity;
  for (const { index } of viewableItems) {
    if (index < low) low = index;
    if (index > high) high = index;
  }
  return { low, high };
}
