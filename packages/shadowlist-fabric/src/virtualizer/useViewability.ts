import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnViewableIndicesChange } from '../ShadowListViewNativeComponent';
import type { ViewabilityConfigCallbackPair, ViewToken } from '../types';
import type { MountedRange } from './mountedRange';
import {
  activeStickyIndexFor,
  viewableRulesFor,
  viewableRange,
  ViewabilityTracker,
} from './viewability';

interface UseViewabilityOptions<ItemT> {
  data: ReadonlyArray<ItemT>;
  keys: ReadonlyArray<string>;
  stickyIndices: ReadonlyArray<number> | undefined;
  pairs: ReadonlyArray<ViewabilityConfigCallbackPair<ItemT>>;
}

interface UseViewabilityResult {
  activeStickyIndex: number;
  viewableRules: number[];
  handleViewableIndicesChange: CodegenTypes.DirectEventHandler<
    OnViewableIndicesChange,
    never
  >;
  recordInteraction: () => void;
}

/*
 * Tracks which items are viewable for each viewability config, and which section header is
 * pinned. Both come from the native viewable ranges event, one range per rule. The first rule
 * is any overlap and picks the pinned header. The rest follow the configs in order.
 */
export function useViewability<ItemT>({
  data,
  keys,
  stickyIndices,
  pairs,
}: UseViewabilityOptions<ItemT>): UseViewabilityResult {
  const [activeStickyIndex, setActiveStickyIndex] = useState(-1);

  const updateActiveStickyIndex = useCallback(
    (rangeLow: number) => {
      if (!stickyIndices || stickyIndices.length === 0) {
        setActiveStickyIndex((previous) => (previous === -1 ? previous : -1));
        return;
      }
      const active = activeStickyIndexFor(stickyIndices, rangeLow);
      setActiveStickyIndex((previous) =>
        previous === active ? previous : active
      );
    },
    [stickyIndices]
  );

  /*
   * The rules only change with the thresholds. The same numbers keep the same array, and
   * native gets no new prop.
   */
  const rulesSignature = pairs
    .map(
      (pair) =>
        `${pair.viewabilityConfig.itemVisiblePercentThreshold ?? ''}:${pair.viewabilityConfig.viewAreaCoveragePercentThreshold ?? ''}`
    )
    .join(',');
  const viewableRules = useMemo(
    () => viewableRulesFor(pairs.map((pair) => pair.viewabilityConfig)),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [rulesSignature]
  );

  /*
   * One tracker per pair. A tracker keeps what it reported and its timer, and picks up a new
   * config or callback in place.
   */
  const trackersRef = useRef<ViewabilityTracker<ItemT>[]>([]);
  const trackers = trackersRef.current;
  while (trackers.length > pairs.length) trackers.pop()!.dispose();
  pairs.forEach((pair, position) => {
    const tracker = trackers[position];
    if (tracker) {
      tracker.config = pair.viewabilityConfig;
      tracker.onViewableItemsChanged = pair.onViewableItemsChanged;
    } else {
      trackers.push(
        new ViewabilityTracker(
          pair.viewabilityConfig,
          pair.onViewableItemsChanged
        )
      );
    }
  });
  useEffect(
    () => () => trackersRef.current.forEach((tracker) => tracker.dispose()),
    []
  );

  // Builds the tokens for a range. The native callback and the data effect both use it.
  const dataRef = useRef({ data, keys });
  dataRef.current = { data, keys };
  const buildViewableItems = useCallback((low: number, high: number) => {
    const { data: currentData, keys: currentKeys } = dataRef.current;
    const viewableItems: ViewToken<ItemT>[] = [];
    for (let index = low; index <= high; index++) {
      const item = currentData[index];
      if (!item) continue;
      viewableItems.push({
        item,
        index,
        key: currentKeys[index]!,
        isViewable: true,
      });
    }
    return viewableItems;
  }, []);

  /*
   * The last reported ranges and the data length at that time. The effect below reuses them
   * on a data change, and the length tells a plain reorder apart from an insert or remove.
   */
  const viewableRangesRef = useRef<{
    viewableRanges: (MountedRange | null)[];
    dataLength: number;
  }>({ viewableRanges: [], dataLength: 0 });

  const handleViewableIndicesChange: CodegenTypes.DirectEventHandler<
    OnViewableIndicesChange,
    never
  > = useCallback(
    (event) => {
      const { ranges } = event.nativeEvent;
      // The sticky overlay shows the section of the top visible row.
      const anyOverlap = viewableRange(ranges[0] ?? -1, ranges[1] ?? -1);
      if (anyOverlap) {
        updateActiveStickyIndex(anyOverlap.low);
      }
      const viewableRanges: (MountedRange | null)[] = [];
      trackersRef.current.forEach((tracker, position) => {
        const low = ranges[2 + position * 2] ?? -1;
        const high = ranges[3 + position * 2] ?? -1;
        const range = viewableRange(low, high);
        viewableRanges.push(range);
        tracker.update(range, buildViewableItems);
      });
      viewableRangesRef.current = {
        viewableRanges,
        dataLength: dataRef.current.data.length,
      };
    },
    [updateActiveStickyIndex, buildViewableItems]
  );

  /*
   * A reorder can change which items sit in the visible range without native sending a new
   * event. Check again whenever data changes.
   */
  useEffect(() => {
    const record = viewableRangesRef.current;
    /*
     * Only a change with the same length can be a plain reorder. An insert or remove shifts
     * indices and the previous range would report the wrong rows. Native sends a fixed event then.
     */
    if (record.dataLength !== data.length) {
      record.dataLength = data.length;
      return;
    }
    trackersRef.current.forEach((tracker, position) => {
      const range = record.viewableRanges[position];
      if (range) tracker.update(range, buildViewableItems);
    });
  }, [data, buildViewableItems]);

  const recordInteraction = useCallback(() => {
    trackersRef.current.forEach((tracker) =>
      tracker.recordInteraction(buildViewableItems)
    );
  }, [buildViewableItems]);

  return {
    activeStickyIndex,
    viewableRules,
    handleViewableIndicesChange,
    recordInteraction,
  };
}
