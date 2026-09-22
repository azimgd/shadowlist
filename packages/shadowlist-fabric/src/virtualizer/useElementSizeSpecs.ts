import { useMemo, useRef } from 'react';
import type { ElementSizeSpec } from '../types';

interface UseElementSizeSpecsOptions<ElementT> {
  data: ReadonlyArray<ElementT>;
  keys: ReadonlyArray<string>;
  getElementSizeSpec:
    | ((element: ElementT, index: number) => ElementSizeSpec | null | undefined)
    | undefined;
  mountedIndices: number[];
  lookaheadRows: number;
}

/*
 * How many rows from the edge of the sent specs the mounted rows may get before we send
 * them again.
 *
 * Sending is costly, and not because of the specs. Any prop change makes Fabric deep copy
 * every other prop, see convertRawProp in propsConversions.h:177. That copies all of
 * elementsAllKeys, measured at 86 us for 100k short keys and 1.88 ms for 100k long ones.
 * It also gives the core a new props pointer, so it compares every key again.
 *
 * Sending every 16 rows paid that constantly. Now we only send when the specs would run out,
 * about once every lookaheadRows minus EDGE_MARGIN rows.
 */
const EDGE_MARGIN = 24;

/*
 * Build the elementsSizeSpecs prop, so native knows the real heights of rows near the
 * screen before React renders them.
 *
 * We send the whole range, not just what changed. A width change like a rotation makes
 * every old height wrong, and native has to measure again from the prop alone. The edge
 * margin above keeps this off the scroll path.
 *
 * Returns an empty string without getElementSizeSpec, which turns the feature off on both sides.
 */
export function useElementSizeSpecs<ElementT extends { id: string }>({
  data,
  keys,
  getElementSizeSpec,
  mountedIndices,
  lookaheadRows,
}: UseElementSizeSpecsOptions<ElementT>): string {
  /*
   * The range last sent. It lives in a ref so the memo below depends on two numbers that
   * only change when the range is recut, not on mountedIndices, which changes every frame.
   */
  const windowRef = useRef({ low: -1, high: -1 });

  if (mountedIndices.length > 0) {
    const first = mountedIndices[0]!;
    const last = mountedIndices[mountedIndices.length - 1]!;
    const mountedLow = Math.min(first, last);
    const mountedHigh = Math.max(first, last);
    const published = windowRef.current;

    /*
     * Recut only when the mounted rows come within EDGE_MARGIN of the sent range's edge, or
     * nothing was sent yet. Until then the sent specs already cover what's about to show.
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
       * A row whose height doesn't come from its text, like one with an inline image,
       * returns nothing and is left out. The core estimates it and measures it natively,
       * so it's fine to describe only some rows.
       */
      const spec = getElementSizeSpec(element, index);
      if (!spec) continue;

      specs.push({ ...spec, key: keys[index]! });
    }

    return specs.length > 0 ? JSON.stringify(specs) : '';
  }, [data, keys, getElementSizeSpec, low, high]);
}
