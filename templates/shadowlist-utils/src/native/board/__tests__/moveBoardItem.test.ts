import { boardListId, columnKeyOfListId } from '../boardIds';
import { moveBetweenColumns, moveWithinColumn } from '../boardMoves';
import { moveBoardItem } from '../moveBoardItem';
import type { BoardColumn, BoardMoveInfo } from '../types';

interface Card {
  id: string;
}

const cards = (...ids: string[]): Card[] => ids.map((id) => ({ id }));

const board = (): BoardColumn<Card>[] => [
  { key: 'todo', title: 'To do', items: cards('a', 'b', 'c') },
  { key: 'doing', title: 'Doing', items: cards('d', 'e') },
  { key: 'done', title: 'Done', items: [] },
];

const keysOf = (columns: ReadonlyArray<BoardColumn<Card>>) =>
  Object.fromEntries(
    columns.map((column) => [column.key, column.items.map((card) => card.id)])
  );

const find = (columns: ReadonlyArray<BoardColumn<Card>>, id: string): Card =>
  columns.flatMap((column) => column.items).find((card) => card.id === id)!;

function move(
  columns: ReadonlyArray<BoardColumn<Card>>,
  id: string,
  sourceColumnKey: string,
  destinationColumnKey: string,
  destinationKey: string,
  destinationIndex = -1
): BoardMoveInfo<Card> {
  return {
    item: find(columns, id),
    sourceColumnKey,
    destinationColumnKey,
    destinationKey,
    destinationIndex,
  };
}

describe('moveBoardItem', () => {
  it('moves an item into another column before the keyed item', () => {
    const columns = board();
    const next = moveBoardItem(
      columns,
      move(columns, 'b', 'todo', 'doing', 'e')
    );
    expect(keysOf(next)).toEqual({
      todo: ['a', 'c'],
      doing: ['d', 'b', 'e'],
      done: [],
    });
  });

  it('appends for an empty destination key, also into an empty column', () => {
    const columns = board();
    const next = moveBoardItem(columns, move(columns, 'a', 'todo', 'done', ''));
    expect(keysOf(next)).toEqual({
      todo: ['b', 'c'],
      doing: ['d', 'e'],
      done: ['a'],
    });
  });

  it('falls back to the index, clamped, for a key no longer in the column', () => {
    const columns = board();
    expect(
      keysOf(
        moveBoardItem(columns, move(columns, 'a', 'todo', 'doing', 'gone', 1))
      ).doing
    ).toEqual(['d', 'a', 'e']);
    expect(
      keysOf(
        moveBoardItem(columns, move(columns, 'a', 'todo', 'doing', 'gone', 9))
      ).doing
    ).toEqual(['d', 'e', 'a']);
    expect(
      keysOf(
        moveBoardItem(columns, move(columns, 'a', 'todo', 'doing', 'gone', -1))
      ).doing
    ).toEqual(['a', 'd', 'e']);
  });

  it('reorders within one column', () => {
    const columns = board();
    const next = moveBoardItem(
      columns,
      move(columns, 'c', 'todo', 'todo', 'a')
    );
    expect(keysOf(next).todo).toEqual(['c', 'a', 'b']);
    const last = moveBoardItem(columns, move(columns, 'a', 'todo', 'todo', ''));
    expect(keysOf(last).todo).toEqual(['b', 'c', 'a']);
  });

  it('never leaves the item in two columns, even when the source is stale', () => {
    const columns = board();
    const next = moveBoardItem(columns, move(columns, 'd', 'todo', 'done', ''));
    expect(keysOf(next)).toEqual({
      todo: ['a', 'b', 'c'],
      doing: ['e'],
      done: ['d'],
    });
  });

  it('keeps the identity of the columns a move leaves alone', () => {
    const columns = board();
    const next = moveBoardItem(
      columns,
      move(columns, 'b', 'todo', 'doing', '')
    );
    expect(next[2]).toBe(columns[2]);
    expect(next[0]).not.toBe(columns[0]);
    expect(next[0]!.title).toBe('To do');
  });

  it('returns the board itself when nothing moved or the column is unknown', () => {
    const columns = board();
    expect(
      moveBoardItem(columns, move(columns, 'b', 'todo', 'todo', 'c'))
    ).toBe(columns);
    expect(
      moveBoardItem(columns, move(columns, 'b', 'todo', 'nowhere', ''))
    ).toBe(columns);
  });

  it('reads keys through the keyExtractor', () => {
    const columns: BoardColumn<{ slug: string }>[] = [
      { key: 'x', title: 'X', items: [{ slug: 'p' }, { slug: 'q' }] },
      { key: 'y', title: 'Y', items: [{ slug: 'r' }] },
    ];
    const next = moveBoardItem(
      columns,
      {
        item: columns[0]!.items[1]!,
        sourceColumnKey: 'x',
        destinationColumnKey: 'y',
        destinationKey: 'r',
        destinationIndex: 0,
      },
      (item) => item.slug
    );
    expect(next.map((column) => column.items.map((item) => item.slug))).toEqual(
      [['p'], ['q', 'r']]
    );
  });

  it('does not change the columns it was given', () => {
    const columns = board();
    moveBoardItem(columns, move(columns, 'b', 'todo', 'doing', 'e'));
    expect(keysOf(columns)).toEqual(keysOf(board()));
  });
});

describe('the moves a column sends', () => {
  const key = (card: Card) => card.id;

  it('turn a reorder into a move before the next item', () => {
    const data = cards('b', 'a', 'c');
    expect(
      moveWithinColumn(
        'todo',
        { sourceIndex: 0, destinationIndex: 1, data },
        key
      )
    ).toEqual({
      item: data[1],
      sourceColumnKey: 'todo',
      destinationColumnKey: 'todo',
      destinationKey: 'c',
      destinationIndex: 1,
    });
    expect(
      moveWithinColumn(
        'todo',
        { sourceIndex: 0, destinationIndex: 2, data },
        key
      )?.destinationKey
    ).toBe('');
    expect(
      moveWithinColumn(
        'todo',
        { sourceIndex: 0, destinationIndex: 5, data },
        key
      )
    ).toBeUndefined();
  });

  it('give a reorder that moveBoardItem applies as the reordered data', () => {
    const columns = board();
    const data = cards('b', 'c', 'a');
    const next = moveBoardItem(
      columns,
      moveWithinColumn(
        'todo',
        { sourceIndex: 0, destinationIndex: 2, data },
        key
      )!
    );
    expect(keysOf(next).todo).toEqual(['b', 'c', 'a']);
  });

  it('map a drop to the column behind the destination list id', () => {
    const boardId = 'board-1';
    const item = { id: 'a' };
    expect(
      moveBetweenColumns(boardId, 'todo', {
        item,
        sourceIndex: 0,
        sourceListId: boardListId(boardId, 'todo'),
        data: cards('b'),
        destinationListId: boardListId(boardId, 'in/review'),
        destinationIndex: 2,
        destinationKey: 'x',
      })
    ).toEqual({
      item,
      sourceColumnKey: 'todo',
      destinationColumnKey: 'in/review',
      destinationKey: 'x',
      destinationIndex: 2,
    });
  });

  it('ignore a drop into a list of another board', () => {
    expect(columnKeyOfListId('board-1', 'board-10/todo')).toBeUndefined();
    expect(
      moveBetweenColumns('board-1', 'todo', {
        item: { id: 'a' },
        sourceIndex: 0,
        sourceListId: 'board-1/todo',
        data: [],
        destinationListId: 'other',
        destinationIndex: 0,
        destinationKey: '',
      })
    ).toBeUndefined();
  });
});
