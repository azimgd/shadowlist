import { useCallback, useMemo, useRef, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnMountedRangeChange } from '../ShadowListViewNativeComponent';
import { SHADOWLIST_OVERSCAN, slTrace, slTraceEnabled } from './helpers';

/*
 * A range of rows, low to high. Mounts mountOverscanRows extra rows on each side of the screen
 * and only re-renders when the screen leaves that range.
 */
export interface MountedRange {
  low: number;
  high: number;
}

export function initialMountedRange(
  size: number,
  initial: number,
  inverted: boolean,
  offsetIndex: number,
  mountOverscanRows: number = SHADOWLIST_OVERSCAN,
  viewPosition: number = 0
): MountedRange {
  if (size <= 0) return { low: -1, high: -1 };
  /*
   * With a starting target, mount around it so it doesn't flash blank. It wins over the
   * inverted list's bottom start.
   * viewPosition decides which side of the target is on screen. Aligned to the start the
   * rows after it fill the screen, centered or at the end the rows before it. Split the
   * mounted rows the same way.
   */
  if (offsetIndex >= 0) {
    const target = Math.min(offsetIndex, size - 1);
    const before = Math.round(viewPosition * initial);
    return {
      low: Math.max(0, target - mountOverscanRows - before),
      high: Math.min(size - 1, target + (initial - before)),
    };
  }
  if (inverted) {
    return { low: Math.max(0, size - initial), high: size - 1 };
  }
  return { low: 0, high: Math.min(initial, size - 1) };
}

export function rangeToIndices(range: MountedRange): number[] {
  if (range.low < 0 || range.high < 0 || range.low > range.high) return [];
  const indices: number[] = [];
  for (let index = range.low; index <= range.high; index++) indices.push(index);
  return indices;
}

/*
 * Indices of both ranges, sorted and without duplicates. While a scroll command is on its
 * way, mount the rows around its target and keep the rows still on screen. Native jumps a
 * frame or more later, and unmounting the visible rows early would show a blank screen.
 */
export function unionRangeIndices(
  first: MountedRange,
  second: MountedRange
): number[] {
  const firstIndices = rangeToIndices(first);
  const secondIndices = rangeToIndices(second);
  if (firstIndices.length === 0) return secondIndices;
  if (secondIndices.length === 0) return firstIndices;
  const [lower, upper] =
    first.low <= second.low ? [first, second] : [second, first];
  if (upper.low <= lower.high + 1) {
    return rangeToIndices({
      low: lower.low,
      high: Math.max(lower.high, upper.high),
    });
  }
  return [...rangeToIndices(lower), ...rangeToIndices(upper)];
}

/*
 * Whether a new scrollIndex should rebuild the mounted rows around it, instead of
 * where native last reported. A negative value means no target, and must not pull a reader
 * who scrolled away back to the start.
 */
export function shouldReseedFromOffsetIndex(
  previousOffsetIndex: number,
  nextOffsetIndex: number
): boolean {
  return nextOffsetIndex !== previousOffsetIndex && nextOffsetIndex >= 0;
}

/*
 * How many appended rows an inverted list at its end mounts on top of its range. A bigger
 * burst, like a reconnect syncing hundreds of messages, moves the range to the tail instead.
 * MAX_FOLLOWED_APPEND in host/MountedRange.hpp is the same number.
 */
export const MAX_FOLLOWED_APPEND = 50;

/*
 * The mounted range from its edge rows' current indices. A range at an edge of the data grows
 * by up to the leading pad to take rows added past it, and a followed tail takes every append.
 * New rows at an edge mount with the new data because this runs in the render that brings it.
 */
export function grownMountedRange(
  lowIndex: number,
  highIndex: number,
  lowAtStart: boolean,
  highAtEnd: boolean,
  size: number,
  mountOverscanRowsLeading: number,
  followTail: boolean
): MountedRange {
  const low = Math.min(lowIndex, highIndex);
  const high = Math.max(lowIndex, highIndex);
  const grownLow = lowAtStart
    ? Math.max(0, low - mountOverscanRowsLeading)
    : low;
  if (followTail && highAtEnd) {
    const tailHigh = size - 1;
    const tailLow = tailHigh - (high - low) - MAX_FOLLOWED_APPEND;
    return { low: Math.max(grownLow, tailLow), high: tailHigh };
  }
  return {
    low: grownLow,
    high: highAtEnd
      ? Math.min(size - 1, high + mountOverscanRowsLeading)
      : high,
  };
}

/*
 * count is the number of rows and indexOfKey a key lookup that takes where the key was as a
 * hint.
 */
interface UseMountedRangeOptions {
  count: number;
  indexOfKey: (key: string, hint: number) => number;
  initialNumToRender: number;
  inverted: boolean;
  followAppends: boolean;
  scrollIndex: number;
  mountOverscanRows: number;
  mountOverscanRowsLeading: number;
}

/*
 * The props that tell native what is mounted, see ShadowListMountDriver.
 */
interface MountedRangeProps {
  mountedLow: number;
  mountedHigh: number;
  mountedSequence: number;
  mountedGeneration: number;
  seedPending: boolean;
}

interface UseMountedRangeResult {
  mountedIndices: number[];
  mountedRangeProps: MountedRangeProps;
  handleMountedRangeChange: CodegenTypes.DirectEventHandler<
    OnMountedRangeChange,
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
 * The range native sent last, by its edge keys, and the sequence that tells native it was
 * applied.
 */
interface MountedKeys {
  lowKey: string;
  highKey: string;
  lowIndex: number;
  highIndex: number;
  lowAtStart: boolean;
  highAtEnd: boolean;
  sequence: number;
}

/*
 * Decides which rows are mounted. Native moves the range along with the visible rows, paces
 * its overscan on display frames and sends each range by its edge keys. This hook applies it,
 * and only works out what has to change in the same render: the initial range, the rows around
 * a new scroll target, and how the edges grow with rows added past them.
 */
export function useMountedRange({
  count,
  indexOfKey,
  initialNumToRender,
  inverted,
  followAppends,
  scrollIndex,
  mountOverscanRows,
  mountOverscanRowsLeading,
}: UseMountedRangeOptions): UseMountedRangeResult {
  /*
   * Null until native first sends a range. Until then the range comes from
   * initialNumToRender, inverted and scrollIndex.
   */
  const [mountedKeys, setMountedKeys] = useState<MountedKeys | null>(null);

  /*
   * Goes up whenever this hook rebuilds the range itself. Native drops a range it built for an
   * older one, and so does the handler below.
   */
  const [generation, setGeneration] = useState(0);

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
      setGeneration(generation + 1);
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
   * jump lands, then the next range replaces them.
   */
  const seedAroundIndex = useCallback((index: number, viewPosition: number) => {
    setCommandSeed({ index, viewPosition });
  }, []);

  /*
   * The stored edge keys as an index range in the current data. The initial range before
   * the first range from native, or when an edge row was deleted.
   */
  const range = useMemo((): MountedRange => {
    if (mountedKeys !== null) {
      /*
       * Where native had the edge rows is right while the data is the same, and close after
       * most changes. The lookup rarely leaves the rows around them.
       */
      const lowIndex = indexOfKey(mountedKeys.lowKey, mountedKeys.lowIndex);
      const highIndex = indexOfKey(mountedKeys.highKey, mountedKeys.highIndex);
      if (lowIndex >= 0 && highIndex >= 0) {
        return grownMountedRange(
          lowIndex,
          highIndex,
          mountedKeys.lowAtStart,
          mountedKeys.highAtEnd,
          count,
          mountOverscanRowsLeading,
          inverted && followAppends
        );
      }
    }
    if (commandSeed !== null) {
      return initialMountedRange(
        count,
        initialNumToRender,
        inverted,
        commandSeed.index,
        mountOverscanRows,
        commandSeed.viewPosition
      );
    }
    return initialMountedRange(
      count,
      initialNumToRender,
      inverted,
      scrollIndex,
      mountOverscanRows
    );
  }, [
    mountedKeys,
    indexOfKey,
    count,
    initialNumToRender,
    inverted,
    followAppends,
    scrollIndex,
    commandSeed,
    mountOverscanRows,
    mountOverscanRowsLeading,
  ]);

  const mountedIndices = useMemo(() => {
    if (commandSeed === null) return rangeToIndices(range);
    const target = initialMountedRange(
      count,
      initialNumToRender,
      inverted,
      commandSeed.index,
      mountOverscanRows,
      commandSeed.viewPosition
    );
    /*
     * A list resting where it opened has had no range from native. Its rows on screen are
     * still the initial ones and stay mounted until the jump lands.
     */
    const onScreen =
      mountedKeys === null
        ? initialMountedRange(
            count,
            initialNumToRender,
            inverted,
            scrollIndex,
            mountOverscanRows
          )
        : range;
    return unionRangeIndices(onScreen, target);
  }, [
    range,
    mountedKeys,
    commandSeed,
    count,
    initialNumToRender,
    inverted,
    scrollIndex,
    mountOverscanRows,
  ]);

  const mountedRangeProps = useMemo<MountedRangeProps>(
    () => ({
      mountedLow: range.low,
      mountedHigh: range.high,
      mountedSequence: mountedKeys?.sequence ?? 0,
      mountedGeneration: generation,
      seedPending: commandSeed !== null,
    }),
    [range, mountedKeys, generation, commandSeed]
  );

  const generationRef = useRef(generation);
  generationRef.current = generation;

  const handleMountedRangeChange: CodegenTypes.DirectEventHandler<
    OnMountedRangeChange,
    never
  > = useCallback((event) => {
    const {
      lowKey,
      highKey,
      lowIndex,
      highIndex,
      lowAtStart,
      highAtEnd,
      sequence,
      landed,
    } = event.nativeEvent;
    // Built on a range this hook has since rebuilt.
    if (event.nativeEvent.generation !== generationRef.current) return;
    // The scroll landed. Returning the same value skips the re-render.
    setCommandSeed((previous) => (previous === null ? previous : null));
    if (landed) return;
    if (slTraceEnabled()) {
      slTrace(`vis apply keys=${lowKey}..${highKey} seq=${sequence}`);
    }
    setMountedKeys({
      lowKey,
      highKey,
      lowIndex,
      highIndex,
      lowAtStart,
      highAtEnd,
      sequence,
    });
  }, []);

  return {
    mountedIndices,
    mountedRangeProps,
    handleMountedRangeChange,
    seedAroundIndex,
  };
}
