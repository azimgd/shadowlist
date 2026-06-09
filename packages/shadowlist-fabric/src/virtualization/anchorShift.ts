import type { MountedRange } from './mountedRange';

/*
 * Content-anchor a mounted range across a data change. On a prepend above the
 * viewport, native shifts its window by +N but only reports it after an async
 * round-trip; until then a stale range blanks the viewport. So, in the same render
 * that adopts the new data, find the new index of the element at range.low and shift
 * the range by that delta. The shift is a union (never shrunk) so the swept-over rows
 * stay mounted; the reband hysteresis (see resolveVisibleIndices) trims the union
 * back to the overscan band once the trail outgrows its allowance. Mid-range inserts
 * move the anchor by nothing and fall back to the native round-trip.
 *
 * Returns the shifted range, or null when no shift applies: nothing mounted, the
 * anchor row no longer exists, or its index is unchanged.
 */
export const resolveRangeAnchorShift = <ElementT>(
  prevData: ReadonlyArray<ElementT>,
  nextData: ReadonlyArray<ElementT>,
  range: MountedRange,
  keyExtractor: (item: ElementT, index: number) => string
): MountedRange | null => {
  if (range.low < 0 || range.low >= prevData.length) return null;

  const anchorKey = keyExtractor(prevData[range.low]!, range.low);
  let newLow = -1;
  for (let index = 0; index < nextData.length; index++) {
    if (keyExtractor(nextData[index]!, index) === anchorKey) {
      newLow = index;
      break;
    }
  }
  if (newLow < 0 || newLow === range.low) return null;

  const delta = newLow - range.low;
  const maxIndex = nextData.length - 1;
  const clamp = (value: number) => Math.min(Math.max(0, value), maxIndex);
  return {
    low: clamp(Math.min(range.low, range.low + delta)),
    high: clamp(Math.max(range.high, range.high + delta)),
  };
};
