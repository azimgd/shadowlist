import { useCallback, useEffect, useRef, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnViewableIndicesChange } from 'shadowlist';
import { slLog } from './helpers';
import type { ViewToken } from '../types';

interface UseViewabilityOptions<ElementT> {
  data: ReadonlyArray<ElementT>;
  keyExtractor: (element: ElementT, index: number) => string;
  stickyHeaderIndices: ReadonlyArray<number> | undefined;
  onViewableItemsChanged:
    | ((info: {
        viewableItems: ViewToken<ElementT>[];
        changed: ViewToken<ElementT>[];
      }) => void)
    | undefined;
}

interface UseViewabilityResult {
  /*
   * Flat index of the section header at the top of the viewport (drives the sticky
   * overlay content); -1 when scrolled above the first section header.
   */
  activeStickyIndex: number;
  handleViewableIndicesChange: CodegenTypes.DirectEventHandler<
    OnViewableIndicesChange,
    never
  >;
}

/*
 * Tracks which items are viewable (emitting onViewableItemsChanged tokens) and which
 * section header is currently pinned (activeStickyIndex), both driven by the native
 * viewable-window event.
 */
export function useViewability<ElementT>({
  data,
  keyExtractor,
  stickyHeaderIndices,
  onViewableItemsChanged,
}: UseViewabilityOptions<ElementT>): UseViewabilityResult {
  const [activeStickyIndex, setActiveStickyIndex] = useState(-1);

  const updateActiveStickyIndex = useCallback(
    (windowLow: number) => {
      if (!stickyHeaderIndices || stickyHeaderIndices.length === 0) {
        setActiveStickyIndex((prev) => (prev === -1 ? prev : -1));
        return;
      }
      let active = -1;
      for (const stickyIndex of stickyHeaderIndices) {
        if (stickyIndex <= windowLow) active = stickyIndex;
        else break;
      }
      setActiveStickyIndex((prev) => (prev === active ? prev : active));
    },
    [stickyHeaderIndices]
  );

  const prevViewableRef = useRef<ViewToken<ElementT>[]>([]);
  /*
   * Last reported index window (ascending, inclusive) and the data length it was
   * reported against; null when no rows are viewable. Lets the effect below recompute
   * viewability for the same window on a data change, with dataLength telling a pure
   * reorder apart from an insert/remove that shifted indices under the window.
   */
  const activeWindowRef = useRef<{
    low: number;
    high: number;
    dataLength: number;
  } | null>(null);

  /*
   * Builds the viewable ViewTokens for an index window. Shared by the native callback
   * and the data-identity effect so the token shape cannot diverge.
   */
  const buildViewableItems = useCallback(
    (windowLow: number, windowHigh: number) => {
      const viewableItems: ViewToken<ElementT>[] = [];
      for (let index = windowLow; index <= windowHigh; index++) {
        const item = data[index];
        if (!item) continue;
        viewableItems.push({
          item,
          index,
          key: keyExtractor(item, index),
          isViewable: true,
        });
      }
      return viewableItems;
    },
    [data, keyExtractor]
  );

  /*
   * Diffs `viewableItems` against the previous emission by key and, if anything
   * changed, updates prevViewableRef and fires onViewableItemsChanged. Shared by both
   * the native callback and the data-identity effect below so the two stay in sync.
   */
  const diffAndEmit = useCallback(
    (viewableItems: ViewToken<ElementT>[]) => {
      if (!onViewableItemsChanged) return;

      const currentKeys = new Set(viewableItems.map((token) => token.key));
      const prevKeys = new Set(
        prevViewableRef.current.map((token) => token.key)
      );

      const changed: ViewToken<ElementT>[] = [
        ...viewableItems.filter((token) => !prevKeys.has(token.key)),
        ...prevViewableRef.current
          .filter((token) => !currentKeys.has(token.key))
          .map((token) => ({ ...token, isViewable: false })),
      ];

      prevViewableRef.current = viewableItems;

      if (changed.length > 0) {
        slLog(
          'js.onViewableItemsChange',
          `viewable=${viewableItems.length}`,
          `changed=${changed.length}`
        );
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
      // Normalise to an ascending window (inverted lists report start > end).
      const windowLow = Math.min(viewableStartIndex, viewableEndIndex);
      const windowHigh = Math.max(viewableStartIndex, viewableEndIndex);

      // Drive sticky-overlay content from the viewable top index to match the pin.
      if (isActive) {
        updateActiveStickyIndex(windowLow);
      }

      activeWindowRef.current = isActive
        ? { low: windowLow, high: windowHigh, dataLength: data.length }
        : null;

      /*
       * Tokens exist only for the onViewableItemsChanged consumer; skip the work when
       * nobody listens (sticky tracking and the window cache above are already done).
       */
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
   * A reorder can change which items occupy an already-reported index window without
   * native re-firing the viewable-index event (the window itself hasn't moved), so
   * recompute viewability whenever `data` changes identity.
   */
  useEffect(() => {
    if (!onViewableItemsChanged) return;
    const window = activeWindowRef.current;
    if (!window) return;

    /*
     * Only a same-length change can be a pure reorder. An insert/remove shifts indices,
     * so replaying the cached window would report the wrong rows (e.g. a prepend's new
     * rows as viewable); native's own corrected event covers those cases.
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
