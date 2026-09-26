import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnVisibleIndicesChange } from 'shadowlist';
import { slTrace, slTraceEnabled } from './helpers';
import {
  initialMountedRange,
  rangeToIndices,
  shouldReseedFromOffsetIndex,
  stepMountedRange,
  unionRangeIndices,
  type MountedRange,
} from './mountedRange';

interface UseMountedRangeOptions {
  keys: ReadonlyArray<string>;
  keyToIndex: ReadonlyMap<string, number>;
  initialElementsSize: number;
  inverted: boolean;
  followAppends: boolean;
  containerOffsetIndex: number;
  overscanRows: number;
  overscanRowsLeading: number;
}

interface UseMountedRangeResult {
  mountedIndices: number[];
  handleVisibleIndicesChange: CodegenTypes.DirectEventHandler<
    OnVisibleIndicesChange,
    never
  >;
  seedAroundIndex: (index: number, viewPosition: number) => void;
}

/*
 * Where a scrollToIndex call is about to scroll to.
 */
interface SeedTarget {
  index: number;
  viewPosition: number;
}

/*
 * How many appended rows an inverted list at its end mounts on top of its range. A bigger
 * burst, like a reconnect syncing hundreds of messages, moves the range to the tail instead.
 */
const MAX_FOLLOWED_APPEND = 50;

/*
 * New overscan rows mounted per end per frame, see stepMountedRange. Rows on screen always
 * mount in the same commit.
 */
const MOUNT_STEP_ROWS = 2;

/*
 * Where the last report wants the range to end up, and what was on screen then, by key so a
 * data change between steps keeps pointing at the same rows.
 */
interface MountTarget {
  lowKey: string;
  highKey: string;
  windowLowKey: string;
  windowHighKey: string;
}

interface MountedKeys {
  lowKey: string;
  highKey: string;
  lowAtStart: boolean;
  highAtEnd: boolean;
}

/*
 * Decides which rows are mounted and moves that range along with what native reports as
 * visible, plus overscan.
 */
export function useMountedRange({
  keys,
  keyToIndex,
  initialElementsSize,
  inverted,
  followAppends,
  containerOffsetIndex,
  overscanRows,
  overscanRowsLeading,
}: UseMountedRangeOptions): UseMountedRangeResult {
  /*
   * Null until native first reports what's visible. Until then the range comes from
   * initialElementsSize, inverted and containerOffsetIndex.
   */
  const [mountedKeys, setMountedKeys] = useState<MountedKeys | null>(null);

  /*
   * containerOffsetIndex is the row the core scrolls to, and we mount around it. If we did
   * that only on mount, a later change would scroll to rows React never mounted and the list
   * stayed blank until native reported again. That's common, for example a chat that finds
   * its first unread message after loading. So treat a change like a mount and rebuild the
   * range around the new target in the same commit that scrolls there.
   */
  const [seededOffsetIndex, setSeededOffsetIndex] =
    useState(containerOffsetIndex);
  if (seededOffsetIndex !== containerOffsetIndex) {
    const reseed = shouldReseedFromOffsetIndex(
      seededOffsetIndex,
      containerOffsetIndex
    );
    setSeededOffsetIndex(containerOffsetIndex);
    if (reseed) {
      setMountedKeys(null);
    }
  }

  /*
   * Target of a scrollToIndex call that hasn't landed yet. The prop always aligns to the
   * start, and the core prefers a command over the prop when a commit has both. So this wins
   * over containerOffsetIndex and survives a prop change until native says the scroll landed.
   */
  const [commandSeed, setCommandSeed] = useState<SeedTarget | null>(null);

  /*
   * Keep the stored range. The rows on screen stay mounted next to the target's until the
   * jump lands, then the next report replaces the range.
   */
  const seedAroundIndex = useCallback((index: number, viewPosition: number) => {
    setCommandSeed({ index, viewPosition });
  }, []);

  /*
   * Turn the stored edge keys into an index range in the current data. Use the initial range
   * before the first report, or when an edge row was deleted.
   */
  const resolveRange = useCallback(
    (edges: MountedKeys | null): MountedRange => {
      if (edges !== null) {
        const lowIndex = keyToIndex.get(edges.lowKey);
        const highIndex = keyToIndex.get(edges.highKey);
        if (lowIndex !== undefined && highIndex !== undefined) {
          /*
           * A range that touches an edge of the data grows to take rows added past it, up to
           * the leading pad. Think a new chat message, a page appended at the end, or history
           * added on top. Mount them in the same render as the data change, instead of waiting
           * a full round trip for native to report them while the screen already shows them.
           */
          const low = Math.min(lowIndex, highIndex);
          const high = Math.max(lowIndex, highIndex);
          /*
           * An inverted list at its end with followAppends. The core scrolls to the newest rows
           * in the same commit, so those must mount, however many came in. Padding from the old
           * edge misses rows when a burst is bigger than the pad, or a second append lands before
           * native reports the first. That shows a blank frame and then a jump.
           * Only a burst over MAX_FOLLOWED_APPEND moves the range to the tail instead of growing
           * it, since a reader just above may still see those rows.
           */
          if (inverted && followAppends && edges.highAtEnd) {
            const tailHigh = keys.length - 1;
            const tailLow = tailHigh - (high - low) - MAX_FOLLOWED_APPEND;
            return {
              low: Math.max(
                edges.lowAtStart ? Math.max(0, low - overscanRowsLeading) : low,
                tailLow
              ),
              high: tailHigh,
            };
          }
          return {
            low: edges.lowAtStart
              ? Math.max(0, low - overscanRowsLeading)
              : low,
            high: edges.highAtEnd
              ? Math.min(keys.length - 1, high + overscanRowsLeading)
              : high,
          };
        }
      }
      if (commandSeed !== null) {
        return initialMountedRange(
          keys.length,
          initialElementsSize,
          inverted,
          commandSeed.index,
          overscanRows,
          commandSeed.viewPosition
        );
      }
      return initialMountedRange(
        keys.length,
        initialElementsSize,
        inverted,
        containerOffsetIndex,
        overscanRows
      );
    },
    [
      keyToIndex,
      keys.length,
      initialElementsSize,
      inverted,
      followAppends,
      containerOffsetIndex,
      commandSeed,
      overscanRows,
      overscanRowsLeading,
    ]
  );

  const mountedIndices = useMemo(() => {
    const current = resolveRange(mountedKeys);
    if (commandSeed === null || mountedKeys === null) {
      return rangeToIndices(current);
    }
    const target = initialMountedRange(
      keys.length,
      initialElementsSize,
      inverted,
      commandSeed.index,
      overscanRows,
      commandSeed.viewPosition
    );
    return unionRangeIndices(current, target);
  }, [
    resolveRange,
    mountedKeys,
    commandSeed,
    keys.length,
    initialElementsSize,
    inverted,
    overscanRows,
  ]);

  /*
   * Mounted keys for an index range, with the edge flags the data change logic above reads.
   */
  const keysOfRange = useCallback(
    (low: number, high: number): MountedKeys => ({
      lowKey: keys[low]!,
      highKey: keys[high]!,
      lowAtStart: low === 0,
      highAtEnd: high === keys.length - 1,
    }),
    [keys]
  );

  /*
   * The range the last report asked for. Reports only come when the visible rows change, so
   * the steps between them are driven from here, one per frame, until the range gets there.
   */
  const mountTargetRef = useRef<MountTarget | null>(null);
  const stepFrameRef = useRef<number | null>(null);
  const latestRef = useRef({ keyToIndex, resolveRange, keysOfRange });
  latestRef.current = { keyToIndex, resolveRange, keysOfRange };

  const stepTowardTarget = useCallback(() => {
    stepFrameRef.current = null;
    setMountedKeys((previous) => {
      const target = mountTargetRef.current;
      const latest = latestRef.current;
      if (target === null || previous === null) return previous;
      const low = latest.keyToIndex.get(target.lowKey);
      const high = latest.keyToIndex.get(target.highKey);
      const windowLow = latest.keyToIndex.get(target.windowLowKey);
      const windowHigh = latest.keyToIndex.get(target.windowHighKey);
      if (
        low === undefined ||
        high === undefined ||
        windowLow === undefined ||
        windowHigh === undefined
      ) {
        mountTargetRef.current = null;
        return previous;
      }
      const current = latest.resolveRange(previous);
      const next = stepMountedRange(
        current,
        { low, high },
        {
          low: Math.min(windowLow, windowHigh),
          high: Math.max(windowLow, windowHigh),
        },
        MOUNT_STEP_ROWS
      );
      if (next.low === low && next.high === high) mountTargetRef.current = null;
      if (next.low === current.low && next.high === current.high)
        return previous;
      return latest.keysOfRange(next.low, next.high);
    });
  }, []);

  // After each commit, take the next step if the range isn't there yet.
  useEffect(() => {
    if (mountTargetRef.current === null || stepFrameRef.current !== null)
      return;
    stepFrameRef.current = requestAnimationFrame(stepTowardTarget);
  });

  useEffect(
    () => () => {
      if (stepFrameRef.current !== null)
        cancelAnimationFrame(stepFrameRef.current);
    },
    []
  );

  /*
   * The last visible range native reported, so we know the scroll direction next time. A
   * ref, since it must never cause a render and only the updater below reads it.
   */
  const lastWindowRef = useRef<{ low: number; high: number } | null>(null);

  const handleVisibleIndicesChange: CodegenTypes.DirectEventHandler<
    OnVisibleIndicesChange,
    never
  > = useCallback(
    (event) => {
      const { visibleStartIndex, visibleEndIndex } = event.nativeEvent;
      if (visibleStartIndex === -1 || visibleEndIndex === -1) return;
      // Inverted lists report start after end, so sort them.
      const windowLow = Math.min(visibleStartIndex, visibleEndIndex);
      const windowHigh = Math.max(visibleStartIndex, visibleEndIndex);
      if (windowLow < 0 || windowHigh >= keys.length) return;

      const lastWindow = lastWindowRef.current;
      lastWindowRef.current = { low: windowLow, high: windowHigh };
      // The scroll landed. Returning the same value skips the re-render.
      setCommandSeed((previous) => (previous === null ? previous : null));
      if (slTraceEnabled()) {
        slTrace(`vis win=${windowLow}..${windowHigh} n=${keys.length}`);
      }

      setMountedKeys((previous) => {
        const current = resolveRange(previous);
        // Already mounted, so skip the re-render.
        if (
          current.low >= 0 &&
          windowLow >= current.low &&
          windowHigh <= current.high
        ) {
          if (previous !== null) return previous;
          /*
           * The initial range still uses indices. Pin it to its edge keys, or a later prepend
           * shifts rows out of it and they remount, losing state like a nested list's scroll
           * position. The mounted rows stay the same, so nothing re-renders.
           */
          return {
            lowKey: keys[current.low]!,
            highKey: keys[current.high]!,
            lowAtStart: current.low === 0,
            highAtEnd: current.high === keys.length - 1,
          };
        }

        /*
         * Mount more rows ahead of where the user is going. The direction comes from how the
         * visible range moved, so normal, inverted and horizontal lists all work the same.
         * A first report, or one that didn't move, pads both sides evenly.
         */
        const movingForward = lastWindow ? windowLow > lastWindow.low : false;
        const movingBackward = lastWindow ? windowLow < lastWindow.low : false;
        const leadingPad = overscanRowsLeading;
        const lowPad = movingBackward ? leadingPad : overscanRows;
        const highPad = movingForward ? leadingPad : overscanRows;

        const targetLow = Math.max(0, windowLow - lowPad);
        const targetHigh = Math.min(keys.length - 1, windowHigh + highPad);
        const { low, high } = stepMountedRange(
          current,
          { low: targetLow, high: targetHigh },
          { low: windowLow, high: windowHigh },
          MOUNT_STEP_ROWS
        );
        mountTargetRef.current =
          low === targetLow && high === targetHigh
            ? null
            : {
                lowKey: keys[targetLow]!,
                highKey: keys[targetHigh]!,
                windowLowKey: keys[windowLow]!,
                windowHighKey: keys[windowHigh]!,
              };
        const { lowKey, highKey, lowAtStart, highAtEnd } = keysOfRange(
          low,
          high
        );
        if (
          previous &&
          previous.lowKey === lowKey &&
          previous.highKey === highKey &&
          previous.lowAtStart === lowAtStart &&
          previous.highAtEnd === highAtEnd
        ) {
          return previous;
        }
        if (slTraceEnabled()) {
          slTrace(
            `vis apply range=${low}..${high} was=${current.low}..${current.high}`
          );
        }
        return { lowKey, highKey, lowAtStart, highAtEnd };
      });
    },
    [keys, resolveRange, keysOfRange, overscanRows, overscanRowsLeading]
  );

  return { mountedIndices, handleVisibleIndicesChange, seedAroundIndex };
}
