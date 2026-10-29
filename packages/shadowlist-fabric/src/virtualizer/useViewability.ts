import { useCallback, useMemo, useRef, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnViewabilityChange } from '../ShadowListViewNativeComponent';
import type { ViewabilityConfigCallbackPair, ViewToken } from '../types';
import type { MountedRange } from './useMountedRange';
import type { RowSource } from './rowSource';
import { viewableRulesFor, viewableTimingsFor } from './viewability';

interface UseViewabilityOptions<ItemT> {
  source: RowSource<ItemT>;
  pairs: ReadonlyArray<ViewabilityConfigCallbackPair<ItemT>>;
}

interface UseViewabilityResult {
  activeStickyIndex: number;
  viewableRules: number[];
  viewableTimings: number[];
  viewabilityInteracted: boolean;
  handleViewabilityChange: CodegenTypes.DirectEventHandler<
    OnViewabilityChange,
    never
  >;
  recordInteraction: () => void;
  visibleRangeRef: { current: MountedRange | null };
}

/*
 * Turns native's viewability reports into the callbacks' tokens. Native works out which keys
 * became viewable or stopped for each config, waits out minimumViewTime and
 * waitForInteraction, and picks the pinned section header. This hook only looks the keys up in
 * the data and calls back.
 */
export function useViewability<ItemT>({
  source,
  pairs,
}: UseViewabilityOptions<ItemT>): UseViewabilityResult {
  const [activeStickyIndex, setActiveStickyIndex] = useState(-1);

  /*
   * The rules and timings only change with the configs' numbers. The same numbers keep the
   * same arrays, and native gets no new prop.
   */
  const configsSignature = pairs
    .map(
      ({ viewabilityConfig: config }) =>
        `${config.itemVisiblePercentThreshold ?? ''}:${config.viewAreaCoveragePercentThreshold ?? ''}:${config.minimumViewTime ?? ''}:${config.waitForInteraction ? 1 : 0}`
    )
    .join(',');
  const { viewableRules, viewableTimings } = useMemo(() => {
    const configs = pairs.map((pair) => pair.viewabilityConfig);
    return {
      viewableRules: viewableRulesFor(configs),
      viewableTimings: viewableTimingsFor(configs),
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [configsSignature]);

  const latestRef = useRef({ source, pairs });
  latestRef.current = { source, pairs };

  /*
   * The keys native holds viewable for each config, and the tokens each config reported last,
   * by key. A row that stops being viewable is reported with the token it had, flagged not
   * viewable.
   */
  const viewableKeysRef = useRef<Set<string>[]>([]);
  const reportedRef = useRef<Map<string, ViewToken<ItemT>>[]>([]);
  // A config native dropped starts empty when it comes back.
  if (viewableKeysRef.current.length > pairs.length) {
    viewableKeysRef.current.length = pairs.length;
    reportedRef.current.length = pairs.length;
  }

  /*
   * The visible rows, any part of which is on screen, from the last report.
   */
  const visibleRangeRef = useRef<MountedRange | null>(null);

  const handleViewabilityChange: CodegenTypes.DirectEventHandler<
    OnViewabilityChange,
    never
  > = useCallback((event) => {
    const {
      activeStickyIndex: active,
      visibleLow,
      visibleHigh,
      changes,
    } = event.nativeEvent;
    visibleRangeRef.current =
      visibleLow >= 0 && visibleHigh >= visibleLow
        ? { low: visibleLow, high: visibleHigh }
        : null;
    setActiveStickyIndex((previous) =>
      previous === active ? previous : active
    );
    const currentSource = latestRef.current.source;
    for (const change of changes) {
      const pair = latestRef.current.pairs[change.config];
      if (!pair) continue;
      const previous =
        reportedRef.current[change.config] ??
        new Map<string, ViewToken<ItemT>>();
      // Native sends what changed. The viewable keys are the previous ones with that applied.
      const viewableKeys = new Set(viewableKeysRef.current[change.config]);
      for (const key of change.removedKeys) viewableKeys.delete(key);
      for (const key of change.addedKeys) viewableKeys.add(key);
      viewableKeysRef.current[change.config] = viewableKeys;
      const reported = new Map<string, ViewToken<ItemT>>();
      const viewableItems: ViewToken<ItemT>[] = [];
      for (const key of viewableKeys) {
        const index = currentSource.indexOfKey(
          key,
          previous.get(key)?.index ?? visibleLow
        );
        const item = index < 0 ? undefined : currentSource.itemAt(index);
        if (index < 0 || item === undefined) continue;
        const token = { item, index, key, isViewable: true };
        viewableItems.push(token);
        reported.set(key, token);
      }
      viewableItems.sort((a, b) => a.index - b.index);
      const changed: ViewToken<ItemT>[] = [];
      for (const key of change.addedKeys) {
        const token = reported.get(key);
        if (token) changed.push(token);
      }
      for (const key of change.removedKeys) {
        const token = previous.get(key);
        if (token) changed.push({ ...token, isViewable: false });
      }
      reportedRef.current[change.config] = reported;
      if (changed.length > 0) {
        pair.onViewableItemsChanged?.({ viewableItems, changed });
      }
    }
  }, []);

  /*
   * Tells native once. Only a config with waitForInteraction needs it, and the others skip the
   * render it costs.
   */
  const [viewabilityInteracted, setViewabilityInteracted] = useState(false);
  const recordInteraction = useCallback(() => {
    const waits = latestRef.current.pairs.some(
      (pair) => pair.viewabilityConfig.waitForInteraction
    );
    if (waits) setViewabilityInteracted(true);
  }, []);

  return {
    activeStickyIndex,
    viewableRules,
    viewableTimings,
    viewabilityInteracted,
    handleViewabilityChange,
    recordInteraction,
    visibleRangeRef,
  };
}
