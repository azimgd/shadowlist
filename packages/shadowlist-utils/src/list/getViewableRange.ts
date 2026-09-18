export interface ViewableRange {
  firstIndex: number;
  lastIndex: number;
}

/**
 * The index span covered by an `onViewableItemsChanged` payload, or `undefined` when
 * nothing is viewable. Does not assume the tokens arrive sorted.
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
