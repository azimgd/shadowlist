import { useCallback, useEffect, useRef, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnViewableIndicesChange } from 'shadowlist';
import type { ViewToken } from '../types';

interface UseViewabilityOptions<ElementT> {
  data: ReadonlyArray<ElementT>;
  keys: ReadonlyArray<string>;
  stickyHeaderIndices: ReadonlyArray<number> | undefined;
  onViewableItemsChanged:
    | ((info: {
        viewableItems: ViewToken<ElementT>[];
        changed: ViewToken<ElementT>[];
      }) => void)
    | undefined;
}

interface UseViewabilityResult {
  activeStickyIndex: number;
  handleViewableIndicesChange: CodegenTypes.DirectEventHandler<
    OnViewableIndicesChange,
    never
  >;
}

/*
 * Tracks which items are visible for onViewableItemsChanged, and which section header is
 * pinned. Both come from the native visible range event.
 */
export function useViewability<ElementT>({
  data,
  keys,
  stickyHeaderIndices,
  onViewableItemsChanged,
}: UseViewabilityOptions<ElementT>): UseViewabilityResult {
  const [activeStickyIndex, setActiveStickyIndex] = useState(-1);

  const updateActiveStickyIndex = useCallback(
    (windowLow: number) => {
      if (!stickyHeaderIndices || stickyHeaderIndices.length === 0) {
        setActiveStickyIndex((previous) => (previous === -1 ? previous : -1));
        return;
      }
      let active = -1;
      for (const stickyIndex of stickyHeaderIndices) {
        if (stickyIndex <= windowLow) active = stickyIndex;
        else break;
      }
      setActiveStickyIndex((previous) =>
        previous === active ? previous : active
      );
    },
    [stickyHeaderIndices]
  );

  const previousViewableRef = useRef<ViewToken<ElementT>[]>([]);
  /*
   * The last reported visible range and the data length at that time, or null when nothing
   * is visible. The effect below reuses it on a data change, and the length tells a plain
   * reorder apart from an insert or remove.
   */
  const activeWindowRef = useRef<{
    low: number;
    high: number;
    dataLength: number;
  } | null>(null);

  // Builds the tokens for a range. The native callback and the data effect both use it.
  const buildViewableItems = useCallback(
    (windowLow: number, windowHigh: number) => {
      const viewableItems: ViewToken<ElementT>[] = [];
      for (let index = windowLow; index <= windowHigh; index++) {
        const item = data[index];
        if (!item) continue;
        viewableItems.push({
          item,
          index,
          key: keys[index]!,
          isViewable: true,
        });
      }
      return viewableItems;
    },
    [data, keys]
  );

  /*
   * Compares the visible items with the last ones by key, and if anything changed, saves them
   * and calls onViewableItemsChanged. Used by the native callback and the data effect below.
   */
  const diffAndEmit = useCallback(
    (viewableItems: ViewToken<ElementT>[]) => {
      if (!onViewableItemsChanged) return;

      const currentKeys = new Set(viewableItems.map((token) => token.key));
      const previousKeys = new Set(
        previousViewableRef.current.map((token) => token.key)
      );

      const changed: ViewToken<ElementT>[] = [
        ...viewableItems.filter((token) => !previousKeys.has(token.key)),
        ...previousViewableRef.current
          .filter((token) => !currentKeys.has(token.key))
          .map((token) => ({ ...token, isViewable: false })),
      ];

      previousViewableRef.current = viewableItems;

      if (changed.length > 0) {
        onViewableItemsChanged({ viewableItems, changed });
      }
    },
    [onViewableItemsChanged]
  );

  const handleViewableIndicesChange: CodegenTypes.DirectEventHandler<
    OnViewableIndicesChange,
    never
  > = useCallback(
    (event) => {
      const { viewableStartIndex, viewableEndIndex } = event.nativeEvent;
      const isActive = viewableStartIndex !== -1 && viewableEndIndex !== -1;
      // Inverted lists report start after end, so sort them.
      const windowLow = Math.min(viewableStartIndex, viewableEndIndex);
      const windowHigh = Math.max(viewableStartIndex, viewableEndIndex);

      // The sticky overlay shows the section of the top visible row.
      if (isActive) {
        updateActiveStickyIndex(windowLow);
      }

      activeWindowRef.current = isActive
        ? { low: windowLow, high: windowHigh, dataLength: data.length }
        : null;

      // Tokens are only for onViewableItemsChanged, so skip them when nobody listens.
      if (!onViewableItemsChanged) return;

      diffAndEmit(isActive ? buildViewableItems(windowLow, windowHigh) : []);
    },
    [
      data,
      onViewableItemsChanged,
      updateActiveStickyIndex,
      buildViewableItems,
      diffAndEmit,
    ]
  );

  /*
   * A reorder can change which items sit in the visible range without native sending a new
   * event, so check again whenever data changes.
   */
  useEffect(() => {
    if (!onViewableItemsChanged) return;
    const window = activeWindowRef.current;
    if (!window) return;

    /*
     * Only a change with the same length can be a plain reorder. An insert or remove shifts
     * indices and the old range would report the wrong rows. Native sends a fixed event then.
     */
    if (window.dataLength !== data.length) {
      window.dataLength = data.length;
      return;
    }

    diffAndEmit(buildViewableItems(window.low, window.high));
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [data]);

  return { activeStickyIndex, handleViewableIndicesChange };
}
