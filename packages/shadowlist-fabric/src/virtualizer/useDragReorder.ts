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
 * Drag-to-reorder: force-mounts the picked-up row while it is off-screen and applies a
 * single array move on drop, handing the result to onReorder.
 */
export function useDragReorder<ElementT>({
  data,
  keyToIndex,
  mountedIndices,
  dragEnabled,
  onReorder,
}: UseDragReorderOptions<ElementT>): UseDragReorderResult {
  /*
   * Resolve a data key to its current index in `data` (-1 if gone). Native
   * identifies drag rows by key; we map back to an index against the live data here so a
   * data change between the gesture and the drop reorders the right rows, not stale ones.
   */
  const indexOfKey = useCallback(
    (key: string) => keyToIndex.get(key) ?? -1,
    [keyToIndex]
  );
  /*
   * Key of the picked-up row (not its index): a data mutation mid-gesture (insert/remove
   * before the dragged row) must not leave the force-mount pointed at a stale index, so we
   * track identity and re-resolve the index below on every render, the same way
   * handleDragEnd re-resolves from/to keys on drop.
   */
  const [draggingKey, setDraggingKey] = useState<string | null>(null);

  /*
   * Current index of the picked-up row, re-resolved against live `data` whenever it
   * changes identity (-1 if the key is no longer present, e.g. it was removed mid-drag).
   */
  const draggingIndex = useMemo(
    () => (draggingKey === null ? -1 : indexOfKey(draggingKey)),
    [draggingKey, indexOfKey]
  );

  // Union the picked-up row's index into the rendered set so it stays mounted.
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

  /*
   * Pickup: keep the picked-up row mounted; data order is unchanged. Resolve the key to
   * its current index for the force-mount union.
   */
  const handleDragStart: CodegenTypes.DirectEventHandler<OnDragStart, never> =
    useCallback((event) => {
      const { key } = event.nativeEvent;
      setDraggingKey(key);
    }, []);

  // Drop: resolve the from/to keys to current indices and apply one array move.
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

  // Release draggingKey if dragging is disabled mid-gesture (drop event may be lost).
  useEffect(() => {
    if (!dragEnabled) {
      setDraggingKey((prev) => (prev === null ? prev : null));
    }
  }, [dragEnabled]);

  return { renderIndices, handleDragStart, handleDragEnd };
}
