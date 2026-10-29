import type { CodegenTypes } from 'react-native';
import {
  afterEach,
  beforeEach,
  describe,
  expect,
  it,
  jest,
} from '@jest/globals';
import TestRenderer, { act } from 'react-test-renderer';
import type { OnDragEnd } from '../ShadowListViewNativeComponent';
import type { MoveItemInfo, MoveItemToListInfo } from '../types';
import { useDragReorder } from '../virtualizer/useDragReorder';
import { useDragBoardChecks } from '../virtualizer/useDragBoardChecks';
import type { RowSource } from '../virtualizer/rowSource';

interface Card {
  id: string;
}

function sourceOf(data: Card[] | null, keys: string[]): RowSource<Card> {
  return {
    count: keys.length,
    itemAt: (index) => data?.[index],
    keyAt: (index) => keys[index],
    indexOfKey: (key) => keys.indexOf(key),
    indexOfItem: (item) => (data ? data.indexOf(item as Card) : -1),
    data,
    keysVersion: 0,
    propKeys: keys,
    nativeSticky: false,
    stickyIndices: null,
  };
}

const cards = (...ids: string[]): Card[] => ids.map((id) => ({ id }));

type DragEndHandler = CodegenTypes.DirectEventHandler<OnDragEnd, never>;

/*
 * Renders the hook and returns its drop handler.
 */
function renderDrop(options: {
  source: RowSource<Card>;
  listId?: string;
  onMoveItem?: (info: MoveItemInfo<Card>) => void;
  onMoveItemToList?: (info: MoveItemToListInfo<Card>) => void;
}): DragEndHandler {
  let handler: DragEndHandler | null = null;
  function Probe() {
    handler = useDragReorder({
      source: options.source,
      mountedIndices: [],
      reorderEnabled: true,
      onMoveItem: options.onMoveItem,
      listId: options.listId,
      onMoveItemToList: options.onMoveItemToList,
    }).handleDragEnd;
    return null;
  }
  act(() => {
    TestRenderer.create(<Probe />);
  });
  if (!handler) {
    throw new Error('the hook did not render');
  }
  return handler;
}

function drop(handler: DragEndHandler, event: OnDragEnd) {
  act(() => {
    handler({ nativeEvent: event } as Parameters<DragEndHandler>[0]);
  });
}

const within = (sourceKey: string, destinationKey: string): OnDragEnd => ({
  sourceKey,
  destinationKey,
  destinationListId: '',
  destinationIndex: -1,
});

describe('useDragReorder onDragEnd', () => {
  let warn: jest.SpiedFunction<typeof console.warn>;

  beforeEach(() => {
    warn = jest.spyOn(console, 'warn').mockImplementation(() => {});
  });

  afterEach(() => {
    warn.mockRestore();
  });

  it('moves a row within its list by key', () => {
    const data = cards('a', 'b', 'c', 'd');
    const onMoveItem = jest.fn<(info: MoveItemInfo<Card>) => void>();
    const handler = renderDrop({
      source: sourceOf(data, ['a', 'b', 'c', 'd']),
      onMoveItem,
    });
    drop(handler, within('a', 'c'));
    expect(onMoveItem).toHaveBeenCalledTimes(1);
    const info = onMoveItem.mock.calls[0]![0];
    expect(info.sourceIndex).toBe(0);
    expect(info.destinationIndex).toBe(2);
    expect(info.data.map((card) => card.id)).toEqual(['b', 'c', 'a', 'd']);
  });

  it('sends nothing for a drop where the row started, a cancel, or a key that is gone', () => {
    const onMoveItem = jest.fn<(info: MoveItemInfo<Card>) => void>();
    const handler = renderDrop({
      source: sourceOf(cards('a', 'b'), ['a', 'b']),
      onMoveItem,
    });
    drop(handler, within('a', 'a'));
    drop(handler, within('gone', 'b'));
    drop(handler, within('a', 'gone'));
    expect(onMoveItem).not.toHaveBeenCalled();
  });

  it('moves a row into another list of the board without it in this list', () => {
    const data = cards('a', 'b', 'c');
    const onMoveItem = jest.fn<(info: MoveItemInfo<Card>) => void>();
    const onMoveItemToList =
      jest.fn<(info: MoveItemToListInfo<Card>) => void>();
    const handler = renderDrop({
      source: sourceOf(data, ['a', 'b', 'c']),
      listId: 'todo',
      onMoveItem,
      onMoveItemToList,
    });
    drop(handler, {
      sourceKey: 'b',
      destinationKey: 'x1',
      destinationListId: 'done',
      destinationIndex: 1,
    });
    expect(onMoveItem).not.toHaveBeenCalled();
    expect(onMoveItemToList).toHaveBeenCalledTimes(1);
    const info = onMoveItemToList.mock.calls[0]![0];
    expect(info.item).toBe(data[1]);
    expect(info.sourceIndex).toBe(1);
    expect(info.sourceListId).toBe('todo');
    expect(info.data.map((card) => card.id)).toEqual(['a', 'c']);
    expect(info.destinationListId).toBe('done');
    expect(info.destinationIndex).toBe(1);
    expect(info.destinationKey).toBe('x1');
  });

  it('passes an empty destinationKey through to append', () => {
    const onMoveItemToList =
      jest.fn<(info: MoveItemToListInfo<Card>) => void>();
    const handler = renderDrop({
      source: sourceOf(cards('a'), ['a']),
      listId: 'todo',
      onMoveItemToList,
    });
    drop(handler, {
      sourceKey: 'a',
      destinationKey: '',
      destinationListId: 'done',
      destinationIndex: 4,
    });
    const info = onMoveItemToList.mock.calls[0]![0];
    expect(info.destinationKey).toBe('');
    expect(info.destinationIndex).toBe(4);
    expect(info.data).toEqual([]);
  });

  it('warns once a drop into another list finds no onMoveItemToList', () => {
    const onMoveItem = jest.fn<(info: MoveItemInfo<Card>) => void>();
    const handler = renderDrop({
      source: sourceOf(cards('a', 'b'), ['a', 'b']),
      onMoveItem,
    });
    drop(handler, {
      sourceKey: 'a',
      destinationKey: '',
      destinationListId: 'done',
      destinationIndex: 0,
    });
    expect(onMoveItem).not.toHaveBeenCalled();
    expect(warn).toHaveBeenCalledTimes(1);
    expect(String(warn.mock.calls[0]![0])).toContain('onMoveItemToList');
  });

  it('warns for a board drop from a list without a data array', () => {
    const onMoveItemToList =
      jest.fn<(info: MoveItemToListInfo<Card>) => void>();
    const handler = renderDrop({
      source: sourceOf(null, ['a', 'b']),
      listId: 'todo',
      onMoveItemToList,
    });
    drop(handler, {
      sourceKey: 'a',
      destinationKey: '',
      destinationListId: 'done',
      destinationIndex: 0,
    });
    expect(onMoveItemToList).not.toHaveBeenCalled();
    expect(warn).toHaveBeenCalledTimes(1);
    expect(String(warn.mock.calls[0]![0])).toContain('SectionList');
  });

  it('never moves rows of a list without a data array', () => {
    const onMoveItem = jest.fn<(info: MoveItemInfo<Card>) => void>();
    const handler = renderDrop({
      source: sourceOf(null, ['a', 'b', 'c']),
      onMoveItem,
    });
    drop(handler, within('a', 'c'));
    expect(onMoveItem).not.toHaveBeenCalled();
    expect(warn).toHaveBeenCalledTimes(1);
    expect(String(warn.mock.calls[0]![0])).toContain('onMoveItem');
  });

  it('sends nothing for a board drop whose row left the data', () => {
    const onMoveItemToList =
      jest.fn<(info: MoveItemToListInfo<Card>) => void>();
    const handler = renderDrop({
      source: sourceOf(cards('a'), ['a']),
      listId: 'todo',
      onMoveItemToList,
    });
    drop(handler, {
      sourceKey: 'gone',
      destinationKey: '',
      destinationListId: 'done',
      destinationIndex: 0,
    });
    expect(onMoveItemToList).not.toHaveBeenCalled();
  });
});

describe('useDragBoardChecks', () => {
  let warn: jest.SpiedFunction<typeof console.warn>;

  beforeEach(() => {
    warn = jest.spyOn(console, 'warn').mockImplementation(() => {});
  });

  afterEach(() => {
    warn.mockRestore();
  });

  function Lists({
    ids,
    board,
  }: {
    ids: Array<string | undefined>;
    board: string;
  }) {
    return (
      <>
        {ids.map((id, index) => (
          <Check key={index} board={board} id={id} />
        ))}
      </>
    );
  }

  function Check({ board, id }: { board: string; id: string | undefined }) {
    useDragBoardChecks(board, id);
    return null;
  }

  it('says nothing for lists with their own ids', () => {
    act(() => {
      TestRenderer.create(<Lists board="clean" ids={['todo', 'done']} />);
    });
    expect(warn).not.toHaveBeenCalled();
  });

  it('warns for a list without a dragListId and for a repeated one', () => {
    let renderer: TestRenderer.ReactTestRenderer | null = null;
    act(() => {
      renderer = TestRenderer.create(
        <Lists board="messy" ids={['todo', undefined, 'todo']} />
      );
    });
    const messages = warn.mock.calls.map((call: unknown[]) => String(call[0]));
    expect(
      messages.some((message) => message.includes('has no dragListId'))
    ).toBe(true);
    expect(
      messages.some((message) =>
        message.includes("share the dragListId 'todo'")
      )
    ).toBe(true);

    // Unmounted lists leave the registry. The same ids mount again cleanly.
    act(() => {
      renderer!.unmount();
    });
    warn.mockClear();
    act(() => {
      TestRenderer.create(<Lists board="messy" ids={['todo']} />);
    });
    expect(warn).not.toHaveBeenCalled();
  });
});
