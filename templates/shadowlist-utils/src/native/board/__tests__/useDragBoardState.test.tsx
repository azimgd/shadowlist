import TestRenderer, { act } from 'react-test-renderer';
import { useBoardId } from '../boardIds';
import type { BoardColumn, BoardMoveInfo } from '../types';
import {
  useDragBoardState,
  type DragBoardState,
  type UseDragBoardStateOptions,
} from '../useDragBoardState';

/*
 * act() warns unless the environment says it is a test, and react-test-renderer warns that
 * it is deprecated outside React Native's test environment.
 */
Object.assign(globalThis, {
  IS_REACT_ACT_ENVIRONMENT: true,
  IS_REACT_NATIVE_TEST_ENVIRONMENT: true,
});

interface Card {
  id: string;
}

const cards = (...ids: string[]): Card[] => ids.map((id) => ({ id }));

const INITIAL: BoardColumn<Card>[] = [
  { key: 'todo', title: 'To do', items: cards('a', 'b') },
  { key: 'done', title: 'Done', items: cards('c') },
];

const keysOf = (columns: ReadonlyArray<BoardColumn<Card>>) =>
  columns.map((column) => column.items.map((card) => card.id));

/*
 * Renders the hook and returns a getter for its latest result and a way to render it again
 * with other options.
 */
function renderBoard(options?: UseDragBoardStateOptions<Card>) {
  let state: DragBoardState<Card> | null = null;
  function Probe(props: { options?: UseDragBoardStateOptions<Card> }) {
    state = useDragBoardState(INITIAL, props.options);
    return null;
  }
  let renderer: TestRenderer.ReactTestRenderer;
  act(() => {
    renderer = TestRenderer.create(<Probe options={options} />);
  });
  return {
    current: () => state!,
    rerender: (next: UseDragBoardStateOptions<Card>) => {
      act(() => {
        renderer.update(<Probe options={next} />);
      });
    },
  };
}

const moveOf = (
  item: Card,
  destinationColumnKey: string,
  destinationKey: string
): BoardMoveInfo<Card> => ({
  item,
  sourceColumnKey: 'todo',
  destinationColumnKey,
  destinationKey,
  destinationIndex: 0,
});

describe('useDragBoardState', () => {
  it('starts from the initial columns', () => {
    const board = renderBoard();
    expect(board.current().columns).toBe(INITIAL);
  });

  it('applies a move to both columns in one state change', () => {
    const board = renderBoard();
    act(() => {
      board.current().moveItem(moveOf(INITIAL[0]!.items[0]!, 'done', 'c'));
    });
    expect(keysOf(board.current().columns)).toEqual([['b'], ['a', 'c']]);
  });

  it('applies two moves in one tick on top of each other', () => {
    const board = renderBoard();
    act(() => {
      const { moveItem } = board.current();
      moveItem(moveOf(INITIAL[0]!.items[0]!, 'done', ''));
      moveItem(moveOf(INITIAL[0]!.items[1]!, 'done', ''));
    });
    expect(keysOf(board.current().columns)).toEqual([[], ['c', 'a', 'b']]);
  });

  it('keeps moveItem stable and calls the latest onMoveItem', () => {
    const first = jest.fn();
    const second = jest.fn();
    const board = renderBoard({ onMoveItem: first });
    const { moveItem } = board.current();
    board.rerender({ onMoveItem: second });
    expect(board.current().moveItem).toBe(moveItem);
    const info = moveOf(INITIAL[0]!.items[1]!, 'done', '');
    act(() => {
      moveItem(info);
    });
    expect(first).not.toHaveBeenCalled();
    expect(second).toHaveBeenCalledWith(info);
  });

  it('reads keys through the keyExtractor option', () => {
    const board = renderBoard({ keyExtractor: (card) => `card-${card.id}` });
    act(() => {
      board.current().moveItem(moveOf(INITIAL[0]!.items[0]!, 'done', ''));
    });
    expect(keysOf(board.current().columns)).toEqual([['b'], ['c', 'a']]);
  });

  it('replaces the columns through setColumns', () => {
    const board = renderBoard();
    const next = [{ key: 'only', title: 'Only', items: cards('z') }];
    act(() => {
      board.current().setColumns(next);
    });
    expect(board.current().columns).toBe(next);
  });
});

describe('useBoardId', () => {
  it('gives each mounted board its own id under the prefix', () => {
    const ids: string[] = [];
    function Probe({ prefix }: { prefix?: string }) {
      ids.push(useBoardId(prefix));
      return null;
    }
    act(() => {
      TestRenderer.create(
        <>
          <Probe prefix="release" />
          <Probe prefix="release" />
          <Probe />
        </>
      );
    });
    expect(new Set(ids).size).toBe(3);
    expect(ids[0]!.startsWith('release-')).toBe(true);
    expect(ids[2]!.startsWith('board-')).toBe(true);
  });
});
