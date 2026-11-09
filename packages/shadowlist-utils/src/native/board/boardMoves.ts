import type { MoveItemInfo, MoveItemToListInfo } from 'shadowlist';
import { columnKeyOfListId } from './boardIds';
import type { BoardMoveInfo } from './types';

/*
 * The board move behind a column's own reorder. The item goes before the one that follows it
 * in the reordered data.
 */
export function moveWithinColumn<ItemT>(
  columnKey: string,
  { data, destinationIndex }: MoveItemInfo<ItemT>,
  keyExtractor: (item: ItemT) => string
): BoardMoveInfo<ItemT> | undefined {
  const item = data[destinationIndex];
  if (item === undefined) {
    return undefined;
  }
  const following = data[destinationIndex + 1];
  return {
    item,
    sourceColumnKey: columnKey,
    destinationColumnKey: columnKey,
    destinationKey: following === undefined ? '' : keyExtractor(following),
    destinationIndex,
  };
}

/*
 * The board move behind a drop into another list, or undefined when that list belongs to
 * another board.
 */
export function moveBetweenColumns<ItemT>(
  boardId: string,
  columnKey: string,
  {
    item,
    destinationListId,
    destinationKey,
    destinationIndex,
  }: MoveItemToListInfo<ItemT>
): BoardMoveInfo<ItemT> | undefined {
  const destinationColumnKey = columnKeyOfListId(boardId, destinationListId);
  if (destinationColumnKey === undefined) {
    return undefined;
  }
  return {
    item,
    sourceColumnKey: columnKey,
    destinationColumnKey,
    destinationKey,
    destinationIndex,
  };
}
