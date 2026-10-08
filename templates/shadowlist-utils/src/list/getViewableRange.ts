export interface ViewableRange {
  firstIndex: number;
  lastIndex: number;
}

/**
 * The index range in an `onViewableItemsChanged` payload, or `undefined` when nothing is
 * viewable. The tokens don't need to be sorted.
 *
 * @example
 * onViewableItemsChanged={({ viewableItems }) => {
 *   const range = getViewableRange(viewableItems);
 *   analytics.track('rows_seen', range);
 * }}
 */
export function getViewableRange(
  viewableItems: ReadonlyArray<{ index: number }>
): ViewableRange | undefined {
  if (viewableItems.length === 0) return undefined;
  let firstIndex = Infinity;
  let lastIndex = -Infinity;
  for (const { index } of viewableItems) {
    if (index < firstIndex) firstIndex = index;
    if (index > lastIndex) lastIndex = index;
  }
  return { firstIndex, lastIndex };
}
