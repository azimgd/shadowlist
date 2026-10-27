/*
 * Board.List imports react-native and shadowlist, which do not load under Node. The mocks draw
 * their components as named host elements, so the tests read the props the board gives them.
 */
jest.mock(
  'react-native',
  () => ({
    Platform: {
      OS: 'ios',
      select: (options: { ios?: unknown }) => options.ios,
    },
    Appearance: {
      getColorScheme: () => 'light',
      addChangeListener: () => ({ remove: () => {} }),
    },
    PlatformColor: (...names: string[]) => names[0],
    StyleSheet: {
      create: <T,>(styles: T) => styles,
      absoluteFill: { position: 'absolute' },
    },
    useWindowDimensions: () => ({ width: 400, height: 1000 }),
    ScrollView: 'ScrollView',
    View: 'View',
    Text: 'Text',
  }),
  { virtual: true }
);
jest.mock('shadowlist', () => ({ DraggableList: 'DraggableList' }), {
  virtual: true,
});

import TestRenderer, { act } from 'react-test-renderer';
import { BoardList, type BoardListProps } from '../BoardList';
import type { BoardColumn } from '../types';

Object.assign(globalThis, {
  IS_REACT_ACT_ENVIRONMENT: true,
  IS_REACT_NATIVE_TEST_ENVIRONMENT: true,
});

interface Card {
  id: string;
}

const cards = (...ids: string[]): Card[] => ids.map((id) => ({ id }));

const COLUMNS: BoardColumn<Card>[] = [
  { key: 'todo', title: 'To do', items: cards('a', 'b', 'c') },
  { key: 'done', title: 'Done', items: [] },
];

const renderItem = () => <></>;

function renderBoard(props: Partial<BoardListProps<Card>> = {}) {
  let renderer: TestRenderer.ReactTestRenderer;
  act(() => {
    renderer = TestRenderer.create(
      <BoardList
        columns={COLUMNS}
        renderItem={renderItem}
        onMoveItem={() => {}}
        testID="board"
        {...props}
      />
    );
  });
  return renderer!;
}

const listsOf = (renderer: TestRenderer.ReactTestRenderer) =>
  renderer.root.findAll((node) => (node.type as unknown) === 'DraggableList');

const scrollOf = (renderer: TestRenderer.ReactTestRenderer) =>
  renderer.root.find((node) => (node.type as unknown) === 'ScrollView');

const bodyOf = (list: TestRenderer.ReactTestInstance) => list.parent!;

const textsOf = (renderer: TestRenderer.ReactTestRenderer) =>
  renderer.root
    .findAll((node) => (node.type as unknown) === 'Text')
    .map((node) => node.props.children);

describe('Board.List', () => {
  it('fills the height it is given by default', () => {
    const renderer = renderBoard();
    const board = scrollOf(renderer);
    expect(board.props.style[0]).toEqual({ flex: 1 });
    for (const list of listsOf(renderer)) {
      expect(bodyOf(list).props.style).toEqual({ flex: 1 });
      expect(list.props.scrollEnabled).toBe(true);
      expect(list.props.onContentSizeChange).toBeUndefined();
    }
  });

  it('draws a footer under each column, outside its list', () => {
    const renderer = renderBoard({
      renderColumnFooter: (column) => (
        <TestSlot testID={`footer-${column.key}`} />
      ),
    });
    for (const column of COLUMNS) {
      const footer = renderer.root.findByProps({
        testID: `footer-${column.key}`,
      });
      const columnView = renderer.root.findByProps({
        testID: `board-column-${column.key}`,
      });
      expect(footer.parent).toBe(columnView);
      const list = listsOf(renderer).find(
        (node) => node.props.testID === `board-list-${column.key}`
      )!;
      expect(list.props.ListFooterComponent).toBeUndefined();
      expect(list.props.data).toBe(column.items);
    }
  });

  it('takes a header of its own and a column style', () => {
    const renderer = renderBoard({
      columnStyle: { backgroundColor: 'tint' },
      renderColumnHeader: (column) => (
        <TestSlot testID={`header-${column.key}`} />
      ),
    });
    const columnView = renderer.root.findByProps({
      testID: 'board-column-todo',
    });
    expect(columnView.props.style[2]).toEqual({ backgroundColor: 'tint' });
    expect(renderer.root.findByProps({ testID: 'header-todo' }).parent).toBe(
      columnView
    );
    expect(textsOf(renderer)).not.toContain('To do');
  });

  it('names an empty column by whether cards can move', () => {
    expect(textsOf(renderBoard())).toContain('Drop cards here');
    const fixed = renderBoard({ onMoveItem: undefined });
    expect(textsOf(fixed)).toContain('No cards');
    for (const list of listsOf(fixed)) {
      expect(list.props.reorderEnabled).toBe(false);
    }
  });

  it('sizes every column to the tallest column within the bounds', () => {
    const renderer = renderBoard({
      columnHeight: 'content',
      minColumnHeight: 100,
      maxColumnHeight: 500,
      estimatedItemHeight: 50,
    });
    const board = scrollOf(renderer);
    expect(board.props.style[0]).toBeUndefined();
    const heights = () =>
      listsOf(renderer).map((list) => bodyOf(list).props.style.height);
    expect(heights()).toEqual([150, 150]);

    const [todo, done] = listsOf(renderer);
    act(() => {
      todo!.props.onContentSizeChange(300, 240);
      done!.props.onContentSizeChange(300, 0);
    });
    expect(heights()).toEqual([240, 240]);
    expect(listsOf(renderer).map((list) => list.props.scrollEnabled)).toEqual([
      false,
      false,
    ]);
    expect(
      listsOf(renderer).every((list) => list.props.nestedScrollEnabled)
    ).toBe(true);

    act(() => {
      listsOf(renderer)[0]!.props.onContentSizeChange(300, 900);
    });
    expect(heights()).toEqual([500, 500]);
    expect(listsOf(renderer).map((list) => list.props.scrollEnabled)).toEqual([
      true,
      false,
    ]);
  });

  it('gives every column 78% of the board by default', () => {
    const renderer = renderBoard();
    const board = scrollOf(renderer);
    expect(board.props.snapToInterval).toBe(312 + 12);
    expect(board.props.contentContainerStyle[1]).toEqual({
      paddingHorizontal: 44,
    });
    for (const column of COLUMNS) {
      const columnView = renderer.root.findByProps({
        testID: `board-column-${column.key}`,
      });
      expect(columnView.props.style[1]).toEqual({ width: 312 });
    }
  });

  it('takes a column width of its own for the columns and the snap', () => {
    const renderer = renderBoard({ columnWidth: 280 });
    const board = scrollOf(renderer);
    expect(board.props.snapToInterval).toBe(280 + 12);
    expect(board.props.contentContainerStyle[1]).toEqual({
      paddingHorizontal: 24,
    });
    for (const column of COLUMNS) {
      const columnView = renderer.root.findByProps({
        testID: `board-column-${column.key}`,
      });
      expect(columnView.props.style[1]).toEqual({ width: 280 });
    }
    act(() => {
      board.props.onLayout({ nativeEvent: { layout: { width: 1000 } } });
    });
    expect(scrollOf(renderer).props.snapToInterval).toBe(280 + 12);
  });

  it('ignores a column width that is not a positive number', () => {
    const renderer = renderBoard({ columnWidth: 0 });
    expect(scrollOf(renderer).props.snapToInterval).toBe(312 + 12);
    const negative = renderBoard({ columnWidth: -50 });
    expect(scrollOf(negative).props.snapToInterval).toBe(312 + 12);
  });

  it('caps the content height at a share of the window by default', () => {
    const renderer = renderBoard({ columnHeight: 'content' });
    act(() => {
      listsOf(renderer)[0]!.props.onContentSizeChange(300, 5000);
    });
    for (const list of listsOf(renderer)) {
      expect(bodyOf(list).props.style.height).toBe(600);
    }
  });
});

function TestSlot(_props: { testID: string }) {
  return null;
}
