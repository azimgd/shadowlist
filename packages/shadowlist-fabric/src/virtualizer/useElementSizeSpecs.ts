import { useMemo, useRef } from 'react';
import type { ElementSizeSpec } from '../types';

interface UseElementSizeSpecsOptions<ElementT> {
  data: ReadonlyArray<ElementT>;
  keyExtractor: (element: ElementT, index: number) => string;
  getElementSizeSpec:
    | ((element: ElementT, index: number) => ElementSizeSpec | null | undefined)
    | undefined;
  // Flat indices currently mounted; the window is built around these.
  mountedIndices: number[];
  // Rows to describe beyond the mounted window on each side.
  lookaheadRows: number;
}

/*
 * How close the mounted window may come to the edge of the published span before the specs
 * are republished, in rows.
 *
 * Republishing is far more expensive than it looks, and the cost has nothing to do with the
 * specs themselves. Changing ANY prop on the list makes Fabric clone the whole props object,
 * and React Native's codegen'd clone deep-copies every prop the update did not mention --
 * `convertRawProp` returns `sourceValue` by value (propsConversions.h:177). So each republish
 * copies the entire `elementsAllKeys` vector: measured at 86 us per copy for 100k short keys
 * and 1.88 ms for 100k realistic long keys. It also changes the props pointer, which is the
 * proof the core uses to skip revalidating the key collection, so it costs a full O(rows)
 * key comparison on top.
 *
 * Republishing on a fixed row quantum paid that every 16 rows of scrolling. Triggering on
 * the window approaching the edge of what is already published pays it only when the
 * predictions would otherwise run out -- roughly once per (lookaheadRows - EDGE_MARGIN) rows.
 */
const EDGE_MARGIN = 24;

/*
 * Build the `elementsSizeSpecs` prop: what the rows around the viewport will measure to,
 * so native can compute their real heights before React ever renders them.
 *
 * The whole window is published rather than the rows that changed since last time. A delta
 * would be smaller, but it would also be the only description native ever had -- and a
 * width change (rotation, split view, a resized window) invalidates every height measured
 * at the old width, so native has to be able to re-measure from the prop alone. Republishing
 * the window makes that recovery automatic; the quantization above is what keeps the cost of
 * doing so off the scroll path.
 *
 * Returns '' when no getElementSizeSpec was supplied, which is the prop's default and
 * disables the feature entirely on both sides.
 */
export function useElementSizeSpecs<ElementT extends { id: string }>({
  data,
  keyExtractor,
  getElementSizeSpec,
  mountedIndices,
  lookaheadRows,
}: UseElementSizeSpecsOptions<ElementT>): string {
  /*
   * The span currently published. Held in a ref so the memo below depends on two numbers
   * that change only when the span is actually re-cut, rather than on mountedIndices, which
   * changes every frame.
   */
  const windowRef = useRef({ low: -1, high: -1 });

  if (mountedIndices.length > 0) {
    const first = mountedIndices[0]!;
    const last = mountedIndices[mountedIndices.length - 1]!;
    const mountedLow = Math.min(first, last);
    const mountedHigh = Math.max(first, last);
    const published = windowRef.current;

    /*
     * Re-cut only when the mounted window has come within EDGE_MARGIN of the edge of what is
     * published, or nothing is published yet. While it sits comfortably inside, the existing
     * specs already describe everything about to be revealed and republishing buys nothing.
     */
    const needsRecut =
      published.low < 0 ||
      mountedLow - EDGE_MARGIN < published.low ||
      mountedHigh + EDGE_MARGIN > published.high;

    if (needsRecut) {
      windowRef.current = {
        low: Math.max(0, mountedLow - lookaheadRows),
        high: mountedHigh + lookaheadRows,
      };
    }
  }

  const { low, high } = windowRef.current;

  return useMemo(() => {
    if (!getElementSizeSpec || low < 0 || data.length === 0) {
      return '';
    }

    const specs: Array<ElementSizeSpec & { key: string }> = [];
    const last = Math.min(high, data.length - 1);

    for (let index = low; index <= last; index++) {
      const element = data[index];
      if (element === undefined) continue;

      /*
       * A row the host cannot describe -- one containing an inline image, or anything whose
       * height is not a function of its text -- returns nothing and is simply left out. The
       * core estimates it and measures it natively, so predicted and unpredicted rows mix
       * freely and a partial description is a perfectly good one.
       */
      const spec = getElementSizeSpec(element, index);
      if (!spec) continue;

      specs.push({ ...spec, key: keyExtractor(element, index) });
    }

    return specs.length > 0 ? JSON.stringify(specs) : '';
  }, [data, keyExtractor, getElementSizeSpec, low, high]);
}
