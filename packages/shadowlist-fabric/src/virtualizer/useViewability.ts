import { useCallback, useRef, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnViewableIndicesChange } from 'shadowlist';
import { slLog } from './helpers';
import type { ViewToken } from '../types';

interface UseViewabilityOptions<ElementT> {
  data: ReadonlyArray<ElementT>;
  keyExtractor: (item: ElementT, index: number) => string;
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

  const handleViewableIndicesChange: CodegenTypes.DirectEventHandler<
    OnViewableIndicesChange,
    never
  > = useCallback(
    (event) => {
      const { viewableStartIndex, viewableEndIndex } = event.nativeEvent;
      // Normalise to an ascending window (inverted lists report start > end).
      const windowLow = Math.min(viewableStartIndex, viewableEndIndex);
      const windowHigh = Math.max(viewableStartIndex, viewableEndIndex);

      // Drive sticky-overlay content from the viewable top index to match the pin.
      if (viewableStartIndex !== -1 && viewableEndIndex !== -1) {
        updateActiveStickyIndex(windowLow);
      }

      if (!onViewableItemsChanged) return;

      const viewableItems: ViewToken<ElementT>[] = [];
      if (viewableStartIndex !== -1 && viewableEndIndex !== -1) {
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
      }

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
          `viewable=[${windowLow}..${windowHigh}]`,
          `changed=${changed.length}`
        );
        onViewableItemsChanged({ viewableItems, changed });
      }
    },
    [data, keyExtractor, onViewableItemsChanged, updateActiveStickyIndex]
  );

  return { activeStickyIndex, handleViewableIndicesChange };
}
