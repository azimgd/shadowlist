import { useCallback, useMemo, useRef, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnVisibleIndicesChange } from 'shadowlist';
import {
  SHADOWLIST_OVERSCAN,
  SHADOWLIST_OVERSCAN_LEADING,
  slTrace,
  slTraceEnabled,
} from './helpers';
import {
  initialMountedRange,
  rangeToIndices,
  type MountedRange,
} from './mountedRange';

interface UseMountedRangeOptions {
  keys: ReadonlyArray<string>;
  keyToIndex: ReadonlyMap<string, number>;
  initialElementsSize: number;
  inverted: boolean;
  followAppends: boolean;
  containerOffsetIndex: number;
}

interface UseMountedRangeResult {
  mountedIndices: number[];
  handleVisibleIndicesChange: CodegenTypes.DirectEventHandler<
    OnVisibleIndicesChange,
    never
  >;
}

/*
 * Rows an inverted list at its end mounts past its range in one append before the range
 * slides onto the tail instead (a reconnect syncing hundreds of messages).
 */
const MAX_FOLLOWED_APPEND = 50;

interface MountedKeys {
  lowKey: string;
  highKey: string;
  lowAtStart: boolean;
  highAtEnd: boolean;
}

/*
 * Owns the virtualization window: which flat indices are mounted and how the range
 * follows the native visible window (+overscan).
 */
export function useMountedRange({
  keys,
  keyToIndex,
  initialElementsSize,
  inverted,
  followAppends,
  containerOffsetIndex,
}: UseMountedRangeOptions): UseMountedRangeResult {
  /*
   * null until the first native visible-window report; the initial range is derived
   * from the seed config (initialElementsSize / inverted / containerOffsetIndex).
   */
  const [mountedKeys, setMountedKeys] = useState<MountedKeys | null>(null);

  /*
   * Resolve the stored edge keys to a [low, high] index range in the current data, or
   * fall back to the seeded initial range before the first report / if an edge key was
   * removed (e.g. the anchored rows were deleted).
   */
  const resolveRange = useCallback(
    (edges: MountedKeys | null): MountedRange => {
      if (edges !== null) {
        const lowIndex = keyToIndex.get(edges.lowKey);
        const highIndex = keyToIndex.get(edges.highKey);
        if (lowIndex !== undefined && highIndex !== undefined) {
          /*
           * A range that reached an edge of the data follows rows added past that edge, up
           * to the leading pad: an incoming message under a chat resting at its newest
           * message, a page appended as the reader reaches the end, history prepended above
           * the top. Those rows mount in the same render as the data change; otherwise they
           * would wait for native to report them visible, a whole JS round trip later, while
           * the viewport is already moving onto them.
           */
          const low = Math.min(lowIndex, highIndex);
          const high = Math.max(lowIndex, highIndex);
          /*
           * An inverted list at its end that follows appends (followAppends): the core scrolls onto the NEWEST
           * rows in the same commit, so those are the ones that must mount, however many
           * arrived. Padding from the stored edge instead leaves the last rows out whenever a
           * burst outgrows the pad, or a second append lands before native reports the first
           * (the stored edge is still the old last row): a blank frame, then a shift as they
           * mount at their real size. Only a burst past MAX_FOLLOWED_APPEND slides the range
           * onto the tail (unmounting rows from its start) rather than growing it without
           * bound: a reader just above the follow band still has those rows on screen.
           */
          if (inverted && followAppends && edges.highAtEnd) {
            const tailHigh = keys.length - 1;
            const tailLow = tailHigh - (high - low) - MAX_FOLLOWED_APPEND;
            return {
              low: Math.max(
                edges.lowAtStart
                  ? Math.max(0, low - SHADOWLIST_OVERSCAN_LEADING)
                  : low,
                tailLow
              ),
              high: tailHigh,
            };
          }
          return {
            low: edges.lowAtStart
              ? Math.max(0, low - SHADOWLIST_OVERSCAN_LEADING)
              : low,
            high: edges.highAtEnd
              ? Math.min(keys.length - 1, high + SHADOWLIST_OVERSCAN_LEADING)
              : high,
          };
        }
      }
      return initialMountedRange(
        keys.length,
        initialElementsSize,
        inverted,
        containerOffsetIndex
      );
    },
    [
      keyToIndex,
      keys.length,
      initialElementsSize,
      inverted,
      followAppends,
      containerOffsetIndex,
    ]
  );

  const mountedIndices = useMemo(
    () => rangeToIndices(resolveRange(mountedKeys)),
    [resolveRange, mountedKeys]
  );

  /*
   * The last window the native side reported, so the direction of travel is known when
   * the range next has to be rebuilt. A ref, not state: it must never itself cause a
   * render, and it is only read inside the updater below.
   */
  const lastWindowRef = useRef<{ low: number; high: number } | null>(null);

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
      if (windowLow < 0 || windowHigh >= keys.length) return;

      const lastWindow = lastWindowRef.current;
      lastWindowRef.current = { low: windowLow, high: windowHigh };
      if (slTraceEnabled()) {
        slTrace(`vis win=${windowLow}..${windowHigh} n=${keys.length}`);
      }

      setMountedKeys((prev) => {
        const current = resolveRange(prev);
        // Already mounted (window within range + overscan): skip the re-render.
        if (
          current.low >= 0 &&
          windowLow >= current.low &&
          windowHigh <= current.high
        ) {
          if (prev !== null) return prev;
          /*
           * The seeded range is still resolved by index. Pin it to the keys at its edges, or
           * a later prepend shifts the mounted rows out of it: they unmount and remount one
           * report later, losing their state (a nested list's scroll position). The rows
           * mounted now are unchanged, so this commit re-renders nothing.
           */
          return {
            lowKey: keys[current.low]!,
            highKey: keys[current.high]!,
            lowAtStart: current.low === 0,
            highAtEnd: current.high === keys.length - 1,
          };
        }

        /*
         * Put the extra runway in front of the user. Direction comes from the window's
         * own movement (which is what the native side reports), so it works the same for
         * normal, inverted and horizontal lists without any of them being special-cased.
         * A first report, or one that did not move, pads both sides evenly.
         */
        const movingForward = lastWindow ? windowLow > lastWindow.low : false;
        const movingBackward = lastWindow ? windowLow < lastWindow.low : false;
        const leadingPad = SHADOWLIST_OVERSCAN_LEADING;
        const lowPad = movingBackward ? leadingPad : SHADOWLIST_OVERSCAN;
        const highPad = movingForward ? leadingPad : SHADOWLIST_OVERSCAN;

        const low = Math.max(0, windowLow - lowPad);
        const high = Math.min(keys.length - 1, windowHigh + highPad);
        const lowKey = keys[low]!;
        const highKey = keys[high]!;
        const lowAtStart = low === 0;
        const highAtEnd = high === keys.length - 1;
        if (
          prev &&
          prev.lowKey === lowKey &&
          prev.highKey === highKey &&
          prev.lowAtStart === lowAtStart &&
          prev.highAtEnd === highAtEnd
        ) {
          return prev;
        }
        if (slTraceEnabled()) {
          slTrace(
            `vis apply range=${low}..${high} was=${current.low}..${current.high}`
          );
        }
        return { lowKey, highKey, lowAtStart, highAtEnd };
      });
    },
    [keys, resolveRange]
  );

  return { mountedIndices, handleVisibleIndicesChange };
}
