import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnMeasuredRangeChange } from '../ShadowListViewNativeComponent';
import { slTrace, slTraceEnabled } from './helpers';
import {
  grownMountedRange,
  initialMountedRange,
  rangeToIndices,
  mountStepForRange,
  reportedMountedRange,
  shouldReseedFromOffsetIndex,
  stepMountedRange,
  unionRangeIndices,
  type MountedRange,
} from './mountedRange';

interface UseMountedRangeOptions {
  keys: ReadonlyArray<string>;
  keyToIndex: ReadonlyMap<string, number>;
  initialNumToRender: number;
  inverted: boolean;
  followAppends: boolean;
  scrollIndex: number;
  mountOverscanRows: number;
  mountOverscanRowsLeading: number;
}

interface UseMountedRangeResult {
  mountedIndices: number[];
  handleMeasuredRangeChange: CodegenTypes.DirectEventHandler<
    OnMeasuredRangeChange,
    never
  >;
  seedAroundIndex: (index: number, viewPosition: number) => void;
}

/*
 * Where a scrollToItem call is about to scroll to.
 */
interface SeedTarget {
  index: number;
  viewPosition: number;
}

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
  measuredLowKey: string;
  measuredHighKey: string;
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
  initialNumToRender,
  inverted,
  followAppends,
  scrollIndex,
  mountOverscanRows,
  mountOverscanRowsLeading,
}: UseMountedRangeOptions): UseMountedRangeResult {
  /*
   * Null until native first reports what's visible. Until then the range comes from
   * initialNumToRender, inverted and scrollIndex.
   */
  const [mountedKeys, setMountedKeys] = useState<MountedKeys | null>(null);

  /*
   * scrollIndex is the row the core scrolls to, and we mount around it. If we did
   * that only on mount, a later change would scroll to rows React never mounted and the list
   * stayed blank until native reported again. That's common, for example a chat that finds
   * its first unread message after loading. So treat a change like a mount and rebuild the
   * range around the new target in the same commit that scrolls there.
   */
  const [seededScrollIndex, setSeededScrollIndex] = useState(scrollIndex);
  if (seededScrollIndex !== scrollIndex) {
    const reseed = shouldReseedFromOffsetIndex(seededScrollIndex, scrollIndex);
    setSeededScrollIndex(scrollIndex);
    if (reseed) {
      setMountedKeys(null);
    }
  }

  /*
   * Target of a scrollToItem call that hasn't landed yet. The prop always aligns to the
   * start, and the core prefers a command over the prop when a commit has both. So this wins
   * over scrollIndex and survives a prop change until native says the scroll landed.
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
          return grownMountedRange(
            lowIndex,
            highIndex,
            edges.lowAtStart,
            edges.highAtEnd,
            keys.length,
            mountOverscanRowsLeading,
            inverted && followAppends
          );
        }
      }
      if (commandSeed !== null) {
        return initialMountedRange(
          keys.length,
          initialNumToRender,
          inverted,
          commandSeed.index,
          mountOverscanRows,
          commandSeed.viewPosition
        );
      }
      return initialMountedRange(
        keys.length,
        initialNumToRender,
        inverted,
        scrollIndex,
        mountOverscanRows
      );
    },
    [
      keyToIndex,
      keys.length,
      initialNumToRender,
      inverted,
      followAppends,
      scrollIndex,
      commandSeed,
      mountOverscanRows,
      mountOverscanRowsLeading,
    ]
  );

  const mountedIndices = useMemo(() => {
    const current = resolveRange(mountedKeys);
    if (commandSeed === null) {
      return rangeToIndices(current);
    }
    const target = initialMountedRange(
      keys.length,
      initialNumToRender,
      inverted,
      commandSeed.index,
      mountOverscanRows,
      commandSeed.viewPosition
    );
    /*
     * A list resting where it opened has had no report that changed its range. Its rows on
     * screen are still the initial ones and stay mounted until the jump lands.
     */
    const onScreen =
      mountedKeys === null
        ? initialMountedRange(
            keys.length,
            initialNumToRender,
            inverted,
            scrollIndex,
            mountOverscanRows
          )
        : current;
    return unionRangeIndices(onScreen, target);
  }, [
    resolveRange,
    mountedKeys,
    commandSeed,
    keys.length,
    initialNumToRender,
    inverted,
    scrollIndex,
    mountOverscanRows,
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
   * The range the last report asked for. Reports only come when the visible rows change.
   * The steps between them are driven from here, one per frame, until the range gets there.
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
      const measuredLow = latest.keyToIndex.get(target.measuredLowKey);
      const measuredHigh = latest.keyToIndex.get(target.measuredHighKey);
      if (
        low === undefined ||
        high === undefined ||
        measuredLow === undefined ||
        measuredHigh === undefined
      ) {
        mountTargetRef.current = null;
        return previous;
      }
      const current = latest.resolveRange(previous);
      const measured = {
        low: Math.min(measuredLow, measuredHigh),
        high: Math.max(measuredLow, measuredHigh),
      };
      const next = stepMountedRange(
        current,
        { low, high },
        measured,
        mountStepForRange(measured, MOUNT_STEP_ROWS)
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
   * The last visible range native reported. It gives the scroll direction next time. A
   * ref, since it must never cause a render and only the updater below reads it.
   */
  const previousMeasuredRef = useRef<{ low: number; high: number } | null>(
    null
  );

  const handleMeasuredRangeChange: CodegenTypes.DirectEventHandler<
    OnMeasuredRangeChange,
    never
  > = useCallback(
    (event) => {
      const { low: measuredLow, high: measuredHigh } = event.nativeEvent;
      if (measuredLow < 0 || measuredHigh < measuredLow) return;
      if (measuredHigh >= keys.length) return;

      const previousMeasured = previousMeasuredRef.current;
      previousMeasuredRef.current = { low: measuredLow, high: measuredHigh };
      // The scroll landed. Returning the same value skips the re-render.
      setCommandSeed((previous) => (previous === null ? previous : null));
      if (slTraceEnabled()) {
        slTrace(`vis win=${measuredLow}..${measuredHigh} n=${keys.length}`);
      }

      setMountedKeys((previous) => {
        const current = resolveRange(previous);
        const measured = { low: measuredLow, high: measuredHigh };
        const reported = reportedMountedRange(
          current,
          measured,
          previousMeasured,
          previous === null,
          keys.length,
          mountOverscanRows,
          mountOverscanRowsLeading,
          MOUNT_STEP_ROWS
        );
        // Already mounted. Skip the re-render.
        if (reported === null) return previous;
        const { low: targetLow, high: targetHigh } = reported.target;
        const { low, high } = reported.range;
        mountTargetRef.current =
          low === targetLow && high === targetHigh
            ? null
            : {
                lowKey: keys[targetLow]!,
                highKey: keys[targetHigh]!,
                measuredLowKey: keys[measuredLow]!,
                measuredHighKey: keys[measuredHigh]!,
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
    [
      keys,
      resolveRange,
      keysOfRange,
      mountOverscanRows,
      mountOverscanRowsLeading,
    ]
  );

  return { mountedIndices, handleMeasuredRangeChange, seedAroundIndex };
}
