import { useCallback, useEffect, useMemo, useState } from 'react';
import type { CodegenTypes } from 'react-native';
import type { OnDragStart, OnDragEnd } from '../ShadowListViewNativeComponent';
import { arrayMove, mergeSortedIndices } from './helpers';
import type { RowSource } from './rowSource';
import type { MoveItemInfo, MoveItemToListInfo } from '../types';

/*
 * listId is this list's dragListId, sent back as sourceListId.
 */
interface UseDragReorderOptions<ItemT> {
  source: RowSource<ItemT>;
  mountedIndices: number[];
  reorderEnabled: boolean;
  onMoveItem: ((info: MoveItemInfo<ItemT>) => void) | undefined;
  listId: string | undefined;
  onMoveItemToList: ((info: MoveItemToListInfo<ItemT>) => void) | undefined;
}

interface UseDragReorderResult {
  renderIndices: number[];
  handleDragStart: CodegenTypes.DirectEventHandler<OnDragStart, never>;
  handleDragEnd: CodegenTypes.DirectEventHandler<OnDragEnd, never>;
}

/*
 * Drag to reorder. Keeps the dragged row mounted while it's off screen, and on drop moves
 * it in the array and passes the result to onMoveItem. A drop in another list of the drag
 * board goes to onMoveItemToList with the row taken out of the array.
 */
export function useDragReorder<ItemT>({
  source,
  mountedIndices,
  reorderEnabled,
  onMoveItem,
  listId,
  onMoveItemToList,
}: UseDragReorderOptions<ItemT>): UseDragReorderResult {
  /*
   * Find a key's current index in data, or -1 if it's gone. Native sends keys. A data
   * change during the drag still moves the right rows. Only runs while dragging.
   */
  const indexOfKey = useCallback(
    (key: string) => source.indexOfKey(key),
    [source]
  );
  /*
   * Key of the dragged row, not its index. An insert or remove during the drag would make an
   * index stale. Look the index up again on every render, like handleDragEnd does on drop.
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
    if (draggingIndex < 0 || draggingIndex >= source.count) {
      return mountedIndices;
    }
    return mergeSortedIndices(mountedIndices, [draggingIndex]);
  }, [mountedIndices, draggingIndex, source.count]);

  // On pickup, keep the row mounted. The data order doesn't change yet.
  const handleDragStart: CodegenTypes.DirectEventHandler<OnDragStart, never> =
    useCallback((event) => {
      const { key } = event.nativeEvent;
      setDraggingKey(key);
    }, []);

  /*
   * On drop, look up both keys and move the row once. A drop in another list of the board
   * names that list. The row leaves this list's data, and the caller adds it to the other
   * before destinationKey, a key of the other list, or at the end when it is ''.
   */
  const handleDragEnd: CodegenTypes.DirectEventHandler<OnDragEnd, never> =
    useCallback(
      (event) => {
        const { sourceKey, destinationKey, destinationListId } =
          event.nativeEvent;
        setDraggingKey(null);
        const sourceIndex = indexOfKey(sourceKey);
        if (destinationListId) {
          const data = source.data;
          const item = sourceIndex === -1 ? undefined : data?.[sourceIndex];
          if (!onMoveItemToList) {
            if (__DEV__) {
              console.warn(
                `ShadowList: a row was dropped into the list '${destinationListId}' of its drag board, ` +
                  'but this list has no onMoveItemToList. Pass it to move the row between lists.'
              );
            }
            return;
          }
          if (!data) {
            if (__DEV__) {
              console.warn(
                'ShadowList: onMoveItemToList needs a list with a data array. SectionList and TreeList ' +
                  'rows cannot move between the lists of a drag board.'
              );
            }
            return;
          }
          if (item !== undefined) {
            onMoveItemToList({
              item,
              sourceIndex,
              sourceListId: listId ?? '',
              data: data.filter((_, index) => index !== sourceIndex),
              destinationListId,
              destinationIndex: event.nativeEvent.destinationIndex,
              destinationKey,
            });
          }
          return;
        }
        const destinationIndex = indexOfKey(destinationKey);
        if (
          sourceIndex !== -1 &&
          destinationIndex !== -1 &&
          sourceIndex !== destinationIndex
        ) {
          onMoveItem?.({
            sourceIndex,
            destinationIndex,
            data: arrayMove(source.data ?? [], sourceIndex, destinationIndex),
          });
        }
      },
      [source, onMoveItem, onMoveItemToList, listId, indexOfKey]
    );

  // Clear draggingKey if dragging is turned off mid drag, since the drop event may never come.
  useEffect(() => {
    if (!reorderEnabled) {
      setDraggingKey((previous) => (previous === null ? previous : null));
    }
  }, [reorderEnabled]);

  return { renderIndices, handleDragStart, handleDragEnd };
}
