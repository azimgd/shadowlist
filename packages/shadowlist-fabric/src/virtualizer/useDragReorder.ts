import { useCallback, useEffect, useMemo, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnDragStart, OnDragEnd } from 'shadowlist';
import { arrayMove, slLog } from './helpers';

interface UseDragReorderOptions<ElementT> {
  data: ReadonlyArray<ElementT>;
  keyExtractor: (element: ElementT, index: number) => string;
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
  keyExtractor,
  mountedIndices,
  dragEnabled,
  onReorder,
}: UseDragReorderOptions<ElementT>): UseDragReorderResult {
  // Resolve a data key to its current index in `data` (-1 if gone). Native
  // identifies drag rows by key; we map back to an index against the live data here so a
  // data change between the gesture and the drop reorders the right rows, not stale ones.
  const indexOfKey = useCallback(
    (key: string) =>
      data.findIndex((element, i) => keyExtractor(element, i) === key),
    [data, keyExtractor]
  );
  /*
   * Index of the picked-up row. Force-mounted so it stays mounted through virtualization
   * while off-screen; the reorder is applied once on drop.
   */
  const [draggingIndex, setDraggingIndex] = useState(-1);

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

  // Pickup: keep the picked-up row mounted; data order is unchanged. Resolve the key to
  // its current index for the force-mount union.
  const handleDragStart: CodegenTypes.DirectEventHandler<OnDragStart, never> =
    useCallback(
      (event) => {
        const { key } = event.nativeEvent;
        setDraggingIndex(indexOfKey(key));
        slLog('js.onDragStart', `key=${key}`);
      },
      [indexOfKey]
    );

  // Drop: resolve the from/to keys to current indices and apply one array move.
  const handleDragEnd: CodegenTypes.DirectEventHandler<OnDragEnd, never> =
    useCallback(
      (event) => {
        const { fromKey, toKey } = event.nativeEvent;
        setDraggingIndex(-1);
        const fromIndex = indexOfKey(fromKey);
        const toIndex = indexOfKey(toKey);
        slLog(
          'js.onDragEnd',
          `from=${fromKey}@${fromIndex}`,
          `to=${toKey}@${toIndex}`
        );
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

  // Release draggingIndex if dragging is disabled mid-gesture (drop event may be lost).
  useEffect(() => {
    if (!dragEnabled) {
      setDraggingIndex((prev) => (prev === -1 ? prev : -1));
    }
  }, [dragEnabled]);

  return { renderIndices, handleDragStart, handleDragEnd };
}
