import { useCallback, useEffect, useMemo, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnDragStart, OnDragEnd } from 'shadowlist';
import { arrayMove } from './helpers';

interface UseDragReorderOptions<ElementT> {
  data: ReadonlyArray<ElementT>;
  keyToIndex: ReadonlyMap<string, number>;
  mountedIndices: number[];
  dragEnabled: boolean;
  onReorder:
    | ((info: { from: number; to: number; data: ElementT[] }) => void)
    | undefined;
}

interface UseDragReorderResult {
  renderIndices: number[];
  handleDragStart: CodegenTypes.DirectEventHandler<OnDragStart, never>;
  handleDragEnd: CodegenTypes.DirectEventHandler<OnDragEnd, never>;
}

/*
 * Drag to reorder. Keeps the dragged row mounted while it's off screen, and on drop moves
 * it in the array and passes the result to onReorder.
 */
export function useDragReorder<ElementT>({
  data,
  keyToIndex,
  mountedIndices,
  dragEnabled,
  onReorder,
}: UseDragReorderOptions<ElementT>): UseDragReorderResult {
  /*
   * Find a key's current index in data, or -1 if it's gone. Native sends keys, so a data
   * change during the drag still moves the right rows.
   */
  const indexOfKey = useCallback(
    (key: string) => keyToIndex.get(key) ?? -1,
    [keyToIndex]
  );
  /*
   * Key of the dragged row, not its index. An insert or remove during the drag would make an
   * index stale, so look the index up again on every render, like handleDragEnd does on drop.
   */
  const [draggingKey, setDraggingKey] = useState<string | null>(null);

  /*
   * Current index of the dragged row, looked up again whenever data changes. -1 if the row
   * was removed during the drag.
   */
  const draggingIndex = useMemo(
    () => (draggingKey === null ? -1 : indexOfKey(draggingKey)),
    [draggingKey, indexOfKey]
  );

  // Add the dragged row to the rendered set so it stays mounted.
  const renderIndices = useMemo(() => {
    if (
      draggingIndex < 0 ||
      draggingIndex >= data.length ||
      mountedIndices.includes(draggingIndex)
    ) {
      return mountedIndices;
    }
    return [...mountedIndices, draggingIndex].sort((a, b) => a - b);
  }, [mountedIndices, draggingIndex, data.length]);

  // On pickup, keep the row mounted. The data order doesn't change yet.
  const handleDragStart: CodegenTypes.DirectEventHandler<OnDragStart, never> =
    useCallback((event) => {
      const { key } = event.nativeEvent;
      setDraggingKey(key);
    }, []);

  // On drop, look up both keys and move the row once.
  const handleDragEnd: CodegenTypes.DirectEventHandler<OnDragEnd, never> =
    useCallback(
      (event) => {
        const { fromKey, toKey } = event.nativeEvent;
        setDraggingKey(null);
        const fromIndex = indexOfKey(fromKey);
        const toIndex = indexOfKey(toKey);
        if (fromIndex !== -1 && toIndex !== -1 && fromIndex !== toIndex) {
          onReorder?.({
            from: fromIndex,
            to: toIndex,
            data: arrayMove(data, fromIndex, toIndex),
          });
        }
      },
      [data, onReorder, indexOfKey]
    );

  // Clear draggingKey if dragging is turned off mid drag, since the drop event may never come.
  useEffect(() => {
    if (!dragEnabled) {
      setDraggingKey((previous) => (previous === null ? previous : null));
    }
  }, [dragEnabled]);

  return { renderIndices, handleDragStart, handleDragEnd };
}
