import { useId } from 'react';

/*
 * The drag board id of one mounted board. Native boards share one namespace across the app.
 * useId makes the id unique to this board, and the prefix only makes it readable in logs.
 */
export function useBoardId(idPrefix = 'board'): string {
  return `${idPrefix}-${useId()}`;
}

export function boardListId(boardId: string, columnKey: string): string {
  return `${boardId}/${columnKey}`;
}

/*
 * The key of the column behind a dragListId of this board, or undefined for a list of
 * another board.
 */
export function columnKeyOfListId(
  boardId: string,
  listId: string
): string | undefined {
  const prefix = `${boardId}/`;
  return listId.startsWith(prefix) ? listId.slice(prefix.length) : undefined;
}
