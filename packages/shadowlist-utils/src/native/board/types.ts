/*
 * One column of a board: the key that names it, the title its header shows and its items in
 * order. Column keys are unique on a board.
 */
export interface BoardColumn<ItemT> {
  key: string;
  title: string;
  items: ReadonlyArray<ItemT>;
}

/*
 * An item that moved on a board, within its column or into another one. It goes into the
 * column keyed destinationColumnKey before the item keyed destinationKey, or at the end when
 * destinationKey is ''. destinationIndex names the same place by index, for a destinationKey
 * that is no longer in that column.
 */
export interface BoardMoveInfo<ItemT> {
  item: ItemT;
  sourceColumnKey: string;
  destinationColumnKey: string;
  destinationKey: string;
  destinationIndex: number;
}
