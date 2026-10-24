import type { ComponentType, ReactElement } from 'react';
import { Platform, Text } from 'react-native';
import { afterEach, describe, expect, it, jest } from '@jest/globals';
import TestRenderer, { act, type ReactTestRenderer } from 'react-test-renderer';
import ShadowList from '../ShadowList';
import ItemActionsView from '../ItemActionsView';
import {
  needsScrollIntoView,
  nextSelectableIndex,
} from '../virtualizer/keySelection';

/*
 * Records the props of every render of the native list and cell views.
 */
type MockProps = Record<string, unknown>;
type MockHostComponent = ComponentType<MockProps>;

const mockRenders = {
  list: [] as MockProps[],
  cells: [] as MockProps[],
};

jest.mock('../ShadowListViewNativeComponent', () => {
  const actual = jest.requireActual<
    typeof import('../ShadowListViewNativeComponent')
  >('../ShadowListViewNativeComponent');
  const Host = actual.default as unknown as MockHostComponent;
  function RecordingListView(props: MockProps) {
    mockRenders.list.push(props);
    return <Host {...props} />;
  }
  return { __esModule: true, ...actual, default: RecordingListView };
});

jest.mock('../ShadowListCellViewNativeComponent', () => {
  const actual = jest.requireActual<
    typeof import('../ShadowListCellViewNativeComponent')
  >('../ShadowListCellViewNativeComponent');
  const Host = actual.default as unknown as MockHostComponent;
  function RecordingCellView(props: MockProps) {
    mockRenders.cells.push(props);
    return <Host {...props} />;
  }
  return { __esModule: true, ...actual, default: RecordingCellView };
});

interface Row {
  id: string;
  header?: boolean;
}

const DATA: Row[] = [
  { id: 'title', header: true },
  { id: 'a' },
  { id: 'b' },
  { id: 'section', header: true },
  { id: 'c' },
];

function keyOf(row: Row) {
  return row.id;
}

function renderRow({ item }: { item: Row }) {
  return <Text>{item.id}</Text>;
}

let renderer: ReactTestRenderer | null = null;

async function mount(tree: ReactElement) {
  await act(async () => {
    renderer = TestRenderer.create(tree);
  });
}

function listProps(): MockProps {
  return mockRenders.list[mockRenders.list.length - 1]!;
}

async function sendToList(name: string, nativeEvent: unknown) {
  await act(async () => {
    (listProps()[name] as (event: { nativeEvent: unknown }) => void)({
      nativeEvent,
    });
  });
}

const originalOS = Platform.OS;

afterEach(async () => {
  await act(async () => {
    renderer?.unmount();
  });
  renderer = null;
  mockRenders.list = [];
  mockRenders.cells = [];
  Platform.OS = originalOS;
});

describe('nextSelectableIndex', () => {
  const rowsOnly = (index: number) => !DATA[index]!.header;

  it('starts at the first or last row that can be selected', () => {
    expect(nextSelectableIndex(-1, 1, DATA.length, rowsOnly)).toBe(1);
    expect(nextSelectableIndex(-1, -1, DATA.length, rowsOnly)).toBe(4);
  });

  it('skips rows that cannot be selected and stops at the ends', () => {
    expect(nextSelectableIndex(2, 1, DATA.length, rowsOnly)).toBe(4);
    expect(nextSelectableIndex(4, -1, DATA.length, rowsOnly)).toBe(2);
    expect(nextSelectableIndex(1, -1, DATA.length, rowsOnly)).toBe(-1);
    expect(nextSelectableIndex(4, 1, DATA.length, rowsOnly)).toBe(-1);
  });
});

describe('needsScrollIntoView', () => {
  it('scrolls for rows off screen or cut off at an edge', () => {
    expect(needsScrollIntoView(5, null)).toBe(true);
    expect(needsScrollIntoView(5, { low: 2, high: 8 })).toBe(false);
    expect(needsScrollIntoView(2, { low: 2, high: 8 })).toBe(true);
    expect(needsScrollIntoView(8, { low: 2, high: 8 })).toBe(true);
    expect(needsScrollIntoView(12, { low: 2, high: 8 })).toBe(true);
  });
});

describe('enableSelectionOnKeyPress', () => {
  it('moves the selection with the arrows and activates it with Return on macOS', async () => {
    Platform.OS = 'macos' as typeof Platform.OS;
    const changes: string[][] = [];
    const activated: string[] = [];
    await mount(
      <ShadowList
        data={DATA}
        renderItem={renderRow}
        enableSelectionOnKeyPress
        shouldSelectItem={({ item }) => !item.header}
        onSelectionChange={(keys) => changes.push(keys)}
        onActivateItem={({ item }) => activated.push(item.id)}
      />
    );
    expect(listProps().focusable).toBe(true);
    expect(listProps().viewableEventEnabled).toBe(true);

    await sendToList('onKeyDown', { key: 'ArrowDown' });
    await sendToList('onKeyDown', { key: 'ArrowDown' });
    await sendToList('onKeyDown', { key: 'ArrowDown' });
    expect(changes).toEqual([['a'], ['b'], ['c']]);

    await sendToList('onKeyDown', { key: 'Enter' });
    expect(activated).toEqual(['c']);
  });

  it('leaves the keyboard alone off macOS', async () => {
    await mount(
      <ShadowList
        data={DATA}
        renderItem={renderRow}
        enableSelectionOnKeyPress
      />
    );
    expect(listProps().focusable).toBeUndefined();
    expect(listProps().onKeyDown).toBeUndefined();
  });

  it('keeps rows shouldSelectItem turns down out of the selection', async () => {
    const changes: string[][] = [];
    let selectHeader = () => {};
    await mount(
      <ShadowList
        data={DATA}
        renderItem={({ item, select }) => {
          if (item.id === 'title') selectHeader = select;
          return <Text>{item.id}</Text>;
        }}
        shouldSelectItem={({ item }) => !item.header}
        onSelectionChange={(keys) => changes.push(keys)}
      />
    );
    await act(async () => {
      selectHeader();
    });
    expect(changes).toEqual([]);
  });
});

describe('onAtEndChange', () => {
  it('reports native at end changes', async () => {
    const reports: boolean[] = [];
    await mount(
      <ShadowList
        data={DATA}
        renderItem={renderRow}
        inverted
        followAppends
        onAtEndChange={(atEnd) => reports.push(atEnd)}
      />
    );
    await sendToList('onAtEndChange', { atEnd: false });
    await sendToList('onAtEndChange', { atEnd: true });
    expect(reports).toEqual([false, true]);
  });
});

const MANY: Row[] = Array.from({ length: 200 }, (_, index) => ({
  id: `r${index}`,
}));

function mountedCellKeys(): string[] {
  const latest = listProps();
  const children = (latest.children as unknown[]).flat(2) as {
    props?: { rowKey?: string };
  }[];
  return children
    .map((child) => child?.props?.rowKey)
    .filter((key): key is string => typeof key === 'string');
}

describe('the mounted range from native', () => {
  it('mounts the range native sends and tells native it applied it', async () => {
    await mount(
      <ShadowList data={MANY} renderItem={renderRow} keyExtractor={keyOf} />
    );
    expect(listProps()).toMatchObject({
      mountedLow: 0,
      mountedHigh: 20,
      mountedSequence: 0,
      mountedGeneration: 0,
      seedPending: false,
      mountOverscanRows: 4,
      mountOverscanRowsLeading: 10,
    });
    await sendToList('onMountedRangeChange', {
      lowKey: 'r30',
      highKey: 'r45',
      lowAtStart: false,
      highAtEnd: false,
      sequence: 3,
      generation: 0,
      landed: false,
    });
    expect(listProps()).toMatchObject({
      mountedLow: 30,
      mountedHigh: 45,
      mountedSequence: 3,
    });
    const keys = mountedCellKeys();
    expect(keys[0]).toBe('r30');
    expect(keys[keys.length - 1]).toBe('r45');
  });

  it('drops a range built before a new scroll target rebuilt the rows', async () => {
    const tree = (scrollIndex: number | undefined) => (
      <ShadowList
        data={MANY}
        renderItem={renderRow}
        keyExtractor={keyOf}
        scrollIndex={scrollIndex}
      />
    );
    await mount(tree(undefined));
    await act(async () => {
      renderer!.update(tree(100));
    });
    expect(listProps()).toMatchObject({
      mountedLow: 96,
      mountedGeneration: 1,
      mountedSequence: 0,
    });
    await sendToList('onMountedRangeChange', {
      lowKey: 'r0',
      highKey: 'r8',
      lowAtStart: true,
      highAtEnd: false,
      sequence: 1,
      generation: 0,
      landed: false,
    });
    expect(listProps()).toMatchObject({ mountedLow: 96, mountedSequence: 0 });
  });
});

describe('viewability and prefetch from native', () => {
  it('turns changed keys into tokens and pins the section header native picks', async () => {
    const reports: { viewable: string[]; changed: [string, boolean][] }[] = [];
    await mount(
      <ShadowList
        data={MANY}
        renderItem={renderRow}
        keyExtractor={keyOf}
        viewabilityConfig={{ itemVisiblePercentThreshold: 50 }}
        onViewableItemsChanged={({ viewableItems, changed }) =>
          reports.push({
            viewable: viewableItems.map((token) => token.key),
            changed: changed.map((token) => [token.key, token.isViewable]),
          })
        }
      />
    );
    expect(listProps()).toMatchObject({
      viewableEventEnabled: true,
      viewableRules: [0, 0, 0.5, 0],
      viewableTimings: [0, 0],
    });
    const change = (
      viewableKeys: string[],
      added: string[],
      removed: string[]
    ) => ({
      activeStickyIndex: -1,
      visibleLow: 0,
      visibleHigh: 3,
      changes: [
        { config: 0, viewableKeys, addedKeys: added, removedKeys: removed },
      ],
    });
    await sendToList(
      'onViewabilityChange',
      change(['r0', 'r1'], ['r0', 'r1'], [])
    );
    await sendToList(
      'onViewabilityChange',
      change(['r1', 'r2'], ['r2'], ['r0'])
    );
    expect(reports).toEqual([
      {
        viewable: ['r0', 'r1'],
        changed: [
          ['r0', true],
          ['r1', true],
        ],
      },
      {
        viewable: ['r1', 'r2'],
        changed: [
          ['r2', true],
          ['r0', false],
        ],
      },
    ]);
  });

  it('passes native prefetch reports to the data source', async () => {
    const prefetched: number[][] = [];
    const cancelled: number[][] = [];
    await mount(
      <ShadowList
        data={MANY}
        renderItem={renderRow}
        keyExtractor={keyOf}
        prefetchDataSource={{
          prefetchItems: (indices) => prefetched.push(indices),
          cancelPrefetchingForItems: (indices) => cancelled.push(indices),
        }}
        prefetchRows={5}
      />
    );
    expect(listProps().prefetchRows).toBe(5);
    await sendToList('onPrefetchChange', { prefetch: [21, 22], cancel: [3] });
    expect(prefetched).toEqual([[21, 22]]);
    expect(cancelled).toEqual([[3]]);
  });
});

describe('a horizontal list styled height auto', () => {
  it('takes the height of its tallest mounted row', async () => {
    await mount(
      <ShadowList
        data={DATA}
        renderItem={renderRow}
        horizontal
        style={{ height: 'auto' }}
      />
    );
    const cells = mockRenders.cells.slice(-DATA.length);
    await act(async () => {
      cells.forEach((cell, index) =>
        (cell.onLayout as (event: unknown) => void)({
          nativeEvent: {
            layout: { x: 0, y: 0, width: 80, height: 40 + index },
          },
        })
      );
    });
    expect(listProps().style).toEqual([
      { height: 'auto' },
      expect.objectContaining({ flexDirection: 'row' }),
      { height: 40 + DATA.length - 1 },
    ]);
  });
});

describe('ItemActionsView', () => {
  it('runs swipe and menu actions with its item', async () => {
    const swiped: string[] = [];
    const chosen: Array<{ item: string | undefined; index: number }> = [];
    await mount(
      <ItemActionsView
        item="record"
        trailingSwipeActions={{
          actions: [
            {
              title: 'Remove',
              style: 'destructive',
              onPress: ({ item }) => {
                swiped.push(item);
              },
            },
          ],
        }}
        contextMenu={{
          title: 'Record',
          actions: [{ title: 'Remove', onPress: (info) => chosen.push(info) }],
        }}
      >
        <Text>Record</Text>
      </ItemActionsView>
    );
    const cell = mockRenders.cells[mockRenders.cells.length - 1]!;
    expect(cell.trailingSwipeActions).toEqual([
      expect.objectContaining({ title: 'Remove', style: 'destructive' }),
    ]);
    expect(cell.contextMenuTitle).toBe('Record');
    await act(async () => {
      (cell.onSwipeAction as (event: unknown) => void)({
        nativeEvent: { leading: false, actionIndex: 0, fullSwipe: false },
      });
      (cell.onContextMenuAction as (event: unknown) => void)({
        nativeEvent: { actionIndex: 0 },
      });
    });
    expect(swiped).toEqual(['record']);
    expect(chosen).toEqual([{ item: 'record', index: -1 }]);
  });
});
