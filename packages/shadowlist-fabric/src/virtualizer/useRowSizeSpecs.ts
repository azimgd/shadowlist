import { useMemo, useRef } from 'react';
import type { ItemSizeSpec } from '../types';
import { nextVersion, type ListStore } from './nativeStore';
import type { RowSource } from './rowSource';

interface UseRowSizeSpecsOptions<ItemT> {
  source: RowSource<ItemT>;
  getItemSizeSpec:
    | ((item: ItemT, index: number) => ItemSizeSpec | null | undefined)
    | undefined;
  mountedIndices: number[];
  lookaheadRows: number;
  store: ListStore | null;
}

/*
 * How native gets the specs: a version of them in the list's native store, or, without the
 * store, a JSON string in the rowSizeSpecs prop. Both empty without getItemSizeSpec, which turns
 * the feature off on both sides.
 */
export interface RowSizeSpecsProps {
  rowSizeSpecs: string;
  sizeSpecsVersion: number;
}

const NO_SPECS: RowSizeSpecsProps = { rowSizeSpecs: '', sizeSpecsVersion: 0 };

/*
 * How many rows from the edge of the sent specs the mounted rows may get before we send
 * them again.
 *
 * Each recut runs getItemSizeSpec over the whole range and gives native a new set to measure.
 * Native measures a few rows per commit, and the text layout cache makes a row it measured
 * before cheap, but only send when the specs would run out, about once every lookaheadRows
 * minus EDGE_MARGIN rows.
 */
const EDGE_MARGIN = 24;

/*
 * The specs of rows near the screen. They tell native the real heights of those rows before
 * React renders them.
 *
 * We send the whole range, not just what changed. A width change like a rotation makes
 * every measured height wrong, and native has to measure again from the specs alone. The edge
 * margin above keeps this off the scroll path.
 *
 * With the native store the spec objects go to native as they are, read field by field, with
 * no JSON string to build and parse, and a later props change leaves them alone.
 */
export function useRowSizeSpecs<ItemT>({
  source,
  getItemSizeSpec,
  mountedIndices,
  lookaheadRows,
  store,
}: UseRowSizeSpecsOptions<ItemT>): RowSizeSpecsProps {
  /*
   * The range last sent. It lives in a ref so the memo below depends on two numbers that
   * only change when the range is recut, not on mountedIndices, which changes every frame.
   */
  const rangeRef = useRef({ low: -1, high: -1 });

  if (mountedIndices.length > 0) {
    const first = mountedIndices[0]!;
    const last = mountedIndices[mountedIndices.length - 1]!;
    const mountedLow = Math.min(first, last);
    const mountedHigh = Math.max(first, last);
    const published = rangeRef.current;

    /*
     * Recut only when the mounted rows come within EDGE_MARGIN of the sent range's edge, or
     * nothing was sent yet. Until then the sent specs already cover what's about to show.
     */
    const needsRecut =
      published.low < 0 ||
      mountedLow - EDGE_MARGIN < published.low ||
      mountedHigh + EDGE_MARGIN > published.high;

    if (needsRecut) {
      rangeRef.current = {
        low: Math.max(0, mountedLow - lookaheadRows),
        high: mountedHigh + lookaheadRows,
      };
    }
  }

  const { low, high } = rangeRef.current;

  return useMemo(() => {
    if (!getItemSizeSpec || low < 0 || source.count === 0) {
      return NO_SPECS;
    }

    const keys: string[] = [];
    const specs: ItemSizeSpec[] = [];
    const last = Math.min(high, source.count - 1);

    for (let index = low; index <= last; index++) {
      const item = source.itemAt(index);
      if (item === undefined) continue;

      /*
       * A row whose height doesn't come from its text, like one with an inline image,
       * returns nothing and is left out. The core estimates it and measures it natively.
       * Describing only some rows is fine.
       */
      const spec = getItemSizeSpec(item, index);
      if (!spec) continue;

      keys.push(source.keyAt(index)!);
      specs.push(spec);
    }

    if (specs.length === 0) return NO_SPECS;
    if (store !== null) {
      const version = nextVersion(store);
      store.module.setSizeSpecs(store.handle, version, keys, specs);
      return { rowSizeSpecs: '', sizeSpecsVersion: version };
    }
    return {
      rowSizeSpecs: JSON.stringify(
        specs.map((spec, position) => ({ ...spec, key: keys[position] }))
      ),
      sizeSpecsVersion: 0,
    };
  }, [source, getItemSizeSpec, low, high, store]);
}
