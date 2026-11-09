import type { BoardColumn, BoardMoveInfo } from './types';

export function defaultBoardKeyExtractor(item: unknown): string {
  return String((item as { id?: unknown }).id);
}

function sameItems<ItemT>(
  previous: ReadonlyArray<ItemT>,
  next: ReadonlyArray<ItemT>
): boolean {
  return (
    previous.length === next.length &&
    previous.every((item, index) => item === next[index])
  );
}

/*
 * Returns the board with move applied in one step. The item leaves every column that holds
 * its key and goes into the destination column before the item keyed destinationKey, at the
 * end for an empty key, or at destinationIndex when that key is no longer there. The item
 * never shows in two columns or in none. Columns the move leaves alone keep their identity,
 * and the board itself comes back unchanged when nothing moved or the destination column is
 * not on it.
 */
export function moveBoardItem<ItemT>(
  columns: ReadonlyArray<BoardColumn<ItemT>>,
  move: BoardMoveInfo<ItemT>,
  keyExtractor: (item: ItemT) => string = defaultBoardKeyExtractor
): ReadonlyArray<BoardColumn<ItemT>> {
  if (!columns.some((column) => column.key === move.destinationColumnKey)) {
    return columns;
  }
  const key = keyExtractor(move.item);
  let changed = false;
  const next = columns.map((column) => {
    const items = column.items.filter((item) => keyExtractor(item) !== key);
    if (column.key === move.destinationColumnKey) {
      const keyed =
        move.destinationKey === ''
          ? items.length
          : items.findIndex(
              (item) => keyExtractor(item) === move.destinationKey
            );
      const at =
        keyed !== -1
          ? keyed
          : Math.max(0, Math.min(move.destinationIndex, items.length));
      items.splice(at, 0, move.item);
    }
    if (sameItems(column.items, items)) {
      return column;
    }
    changed = true;
    return { ...column, items };
  });
  return changed ? next : columns;
}
