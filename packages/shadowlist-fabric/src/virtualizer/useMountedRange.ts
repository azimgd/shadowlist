import { useCallback, useMemo, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnVisibleIndicesChange } from 'shadowlist';
import { SHADOWLIST_OVERSCAN, slLog } from './helpers';
import {
  initialMountedRange,
  rangeToIndices,
  type MountedRange,
} from './mountedRange';

interface UseMountedRangeOptions<ElementT> {
  data: ReadonlyArray<ElementT>;
  keyExtractor: (item: ElementT, index: number) => string;
  initialElementsSize: number;
  inverted: boolean;
  containerOffsetIndex: number;
}

interface UseMountedRangeResult {
  // Flat indices currently mounted (visible window + overscan, content-anchored).
  mountedIndices: number[];
  // Native visible-window callback that advances the mounted range.
  handleVisibleIndicesChange: CodegenTypes.DirectEventHandler<
    OnVisibleIndicesChange,
    never
  >;
}

// The mounted range stored by the keys at its edges, so it survives data changes.
interface MountedKeys {
  lowKey: string;
  highKey: string;
}

/*
 * Owns the virtualization window: which flat indices are mounted and how the range
 * follows the native visible window (+overscan).
 */
export function useMountedRange<ElementT extends { id: string }>({
  data,
  keyExtractor,
  initialElementsSize,
  inverted,
  containerOffsetIndex,
}: UseMountedRangeOptions<ElementT>): UseMountedRangeResult {
  // null until the first native visible-window report; the initial range is derived
  // from the seed config (initialElementsSize / inverted / containerOffsetIndex).
  const [mountedKeys, setMountedKeys] = useState<MountedKeys | null>(null);

  // key -> first index, rebuilt only when the data array identity changes. Resolves a
  // stored edge key back to its current index in O(1); first occurrence wins on a
  // duplicate key, matching the core's reconcile semantics.
  //
  // This is an O(N) full rescan on every data mutation, including a single append or
  // prepend. A plain array gives no cheap diff signal (no keyed diffing, no way to tell
  // "one item added at the end" from "everything changed" without already walking it),
  // so a genuinely incremental update would need extra bookkeeping (e.g. diffing against
  // the previous array) for a case that's already cheap in absolute terms (one Map build
  // per data change, not per row). Accepted as-is rather than adding that complexity.
  const keyToIndex = useMemo(() => {
    const map = new Map<string, number>();
    for (let index = 0; index < data.length; index++) {
      const key = keyExtractor(data[index]!, index);
      if (!map.has(key)) map.set(key, index);
    }
    return map;
  }, [data, keyExtractor]);

  // Resolve the stored edge keys to a [low, high] index range in the current data, or
  // fall back to the seeded initial range before the first report / if an edge key was
  // removed (e.g. the anchored rows were deleted).
  const resolveRange = useCallback(
    (keys: MountedKeys | null): MountedRange => {
      if (keys !== null) {
        const lowIndex = keyToIndex.get(keys.lowKey);
        const highIndex = keyToIndex.get(keys.highKey);
        if (lowIndex !== undefined && highIndex !== undefined) {
          return {
            low: Math.min(lowIndex, highIndex),
            high: Math.max(lowIndex, highIndex),
          };
        }
      }
      return initialMountedRange(
        data.length,
        initialElementsSize,
        inverted,
        containerOffsetIndex
      );
    },
    [
      keyToIndex,
      data.length,
      initialElementsSize,
      inverted,
      containerOffsetIndex,
    ]
  );

  const mountedIndices = useMemo(
    () => rangeToIndices(resolveRange(mountedKeys)),
    [resolveRange, mountedKeys]
  );

  const handleVisibleIndicesChange: CodegenTypes.DirectEventHandler<
    OnVisibleIndicesChange,
    never
  > = useCallback(
    (event) => {
      const { visibleStartIndex, visibleEndIndex } = event.nativeEvent;
      if (visibleStartIndex === -1 || visibleEndIndex === -1) return;
      // Normalise to an ascending window (inverted lists report start > end).
      const windowLow = Math.min(visibleStartIndex, visibleEndIndex);
      const windowHigh = Math.max(visibleStartIndex, visibleEndIndex);
      if (windowLow < 0 || windowHigh >= data.length) return;

      setMountedKeys((prev) => {
        const current = resolveRange(prev);
        // Already mounted (window within range + overscan): skip the re-render.
        if (
          current.low >= 0 &&
          windowLow >= current.low &&
          windowHigh <= current.high
        ) {
          slLog(
            'js.onVisibleIndicesChange skip (in range)',
            `window=[${windowLow}..${windowHigh}]`,
            `range=[${current.low}..${current.high}]`
          );
          return prev;
        }
        const low = Math.max(0, windowLow - SHADOWLIST_OVERSCAN);
        const high = Math.min(
          data.length - 1,
          windowHigh + SHADOWLIST_OVERSCAN
        );
        const lowKey = keyExtractor(data[low]!, low);
        const highKey = keyExtractor(data[high]!, high);
        if (prev && prev.lowKey === lowKey && prev.highKey === highKey) {
          return prev;
        }
        slLog(
          'js.onVisibleIndicesChange apply',
          `window=[${windowLow}..${windowHigh}]`,
          `range=[${low}..${high}]`
        );
        return { lowKey, highKey };
      });
    },
    [data, keyExtractor, resolveRange]
  );

  return { mountedIndices, handleVisibleIndicesChange };
}
