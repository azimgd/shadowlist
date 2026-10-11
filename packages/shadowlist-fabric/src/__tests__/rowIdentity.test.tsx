import { afterEach, describe, expect, it, jest } from '@jest/globals';
import { useState, type ComponentType, type ReactElement } from 'react';
import { Text, View } from 'react-native';
import TestRenderer, { act, type ReactTestRenderer } from 'react-test-renderer';
import ShadowList from '../ShadowList';
import SectionList from '../SectionList';
import type { RenderItemInfo } from '../types';

/*
 * Records every render of the native list and row views. A row view renders only when its
 * CellRenderer ran, which happens when one of the row props changed identity.
 */
type MockProps = Record<string, unknown>;
type MockHostComponent = ComponentType<MockProps>;

const mockRenders = {
  list: [] as Array<Record<string, unknown>>,
  rows: 0,
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
    mockRenders.rows++;
    return <Host {...props} />;
  }
  return { __esModule: true, ...actual, default: RecordingCellView };
});

interface Row {
  id: string;
  title: string;
}

const DATA: Row[] = Array.from({ length: 30 }, (_value, index) => ({
  id: `row-${index}`,
  title: `Row ${index}`,
}));

let itemRenders = 0;

function renderRow({ item }: Pick<RenderItemInfo<Row>, 'item'>) {
  itemRenders++;
  return <Text>{item.title}</Text>;
}

function keyOf(row: Row) {
  return row.id;
}

function Separator() {
  return <View />;
}

function Header() {
  return <Text>Header</Text>;
}

const noop = () => {};
const swipeActions = () => ({
  actions: [{ title: 'Delete', style: 'destructive' as const, onPress: noop }],
});
const contextMenu = () => ({ actions: [{ title: 'Copy', onPress: noop }] });
const prefetchDataSource = {
  prefetchItems: noop,
  cancelPrefetchingForItems: noop,
};
const contentContainerStyle = { paddingVertical: 8, paddingHorizontal: 12 };
const columnWrapperStyle = { columnGap: 4 };
const viewabilityConfig = { itemVisiblePercentThreshold: 50 };
const stickyIndices = [0];
const renderStickyHeaderOverlay = () => <Text>Sticky</Text>;

/*
 * Props that are equal on every render but may come back as new objects from a careless
 * parent: an inline separator React element and an inline header React element.
 */
function ShadowListParent({ inlineElements }: { inlineElements: boolean }) {
  const [, setTick] = useState(0);
  rerenderParent = () => setTick((tick) => tick + 1);
  return (
    <ShadowList
      data={DATA}
      renderItem={renderRow}
      keyExtractor={keyOf}
      numberOfColumns={2}
      ItemSeparatorComponent={inlineElements ? <View /> : Separator}
      ListHeaderComponent={inlineElements ? <Header /> : Header}
      contentContainerStyle={contentContainerStyle}
      columnWrapperStyle={columnWrapperStyle}
      allowsMultipleSelection
      onSelectionChange={noop}
      leadingSwipeActionsForItem={swipeActions}
      trailingSwipeActionsForItem={swipeActions}
      contextMenuForItem={contextMenu}
      prefetchDataSource={prefetchDataSource}
      trackItemSizes
      extraData={1}
      keyboardShouldPersistTaps="handled"
      keyboardDismissMode="on-drag"
      viewabilityConfig={viewabilityConfig}
      onViewableItemsChanged={noop}
      stickyIndices={stickyIndices}
      renderStickyHeaderOverlay={renderStickyHeaderOverlay}
      onRefresh={noop}
      onStartReached={noop}
      onEndReached={noop}
      onScrollBeginDrag={noop}
      onContentSizeChange={noop}
    />
  );
}

interface Section {
  key: string;
  data: Row[];
}

const SECTIONS: Section[] = [
  { key: 'a', data: DATA.slice(0, 10) },
  { key: 'b', data: DATA.slice(10, 20) },
];

function renderSectionHeader({ section }: { section: Section }) {
  return <Text>{section.key}</Text>;
}

function SectionListParent({ inlineElements }: { inlineElements: boolean }) {
  const [, setTick] = useState(0);
  rerenderParent = () => setTick((tick) => tick + 1);
  return (
    <SectionList
      sections={SECTIONS}
      renderItem={renderRow}
      renderSectionHeader={renderSectionHeader}
      keyExtractor={keyOf}
      ItemSeparatorComponent={inlineElements ? <View /> : Separator}
      SectionSeparatorComponent={inlineElements ? <View /> : Separator}
      contentContainerStyle={contentContainerStyle}
      onRefresh={noop}
      trackItemSizes
      onEndReached={noop}
    />
  );
}

let rerenderParent = () => {};
let renderer: ReactTestRenderer | null = null;

async function mount(tree: ReactElement) {
  await act(async () => {
    renderer = TestRenderer.create(tree);
  });
}

/*
 * Props the list rebuilds on every render by design. Everything else must keep its identity.
 */
const REBUILT_LIST_PROPS = new Set(['children', 'style']);

function changedListProps(): string[] {
  const { list } = mockRenders;
  const previous = list[list.length - 2]!;
  const current = list[list.length - 1]!;
  const names = new Set([...Object.keys(previous), ...Object.keys(current)]);
  return [...names].filter(
    (name) => !REBUILT_LIST_PROPS.has(name) && previous[name] !== current[name]
  );
}

async function expectStableRerender() {
  const rowsBefore = mockRenders.rows;
  const itemsBefore = itemRenders;
  const listBefore = mockRenders.list.length;
  await act(async () => {
    rerenderParent();
  });
  expect(mockRenders.list.length).toBe(listBefore + 1);
  expect(changedListProps()).toEqual([]);
  expect(mockRenders.rows - rowsBefore).toBe(0);
  expect(itemRenders - itemsBefore).toBe(0);
}

afterEach(async () => {
  await act(async () => {
    renderer?.unmount();
  });
  renderer = null;
  mockRenders.list = [];
  mockRenders.rows = 0;
  itemRenders = 0;
});

describe('a parent re-render with unchanged props', () => {
  it.each([false, true])(
    'leaves ShadowList rows alone (inline React elements %s)',
    async (inlineElements) => {
      await mount(<ShadowListParent inlineElements={inlineElements} />);
      expect(mockRenders.rows).toBeGreaterThan(0);
      expect(itemRenders).toBeGreaterThan(0);
      await expectStableRerender();
      await expectStableRerender();
    }
  );

  it.each([false, true])(
    'leaves SectionList rows alone (inline React elements %s)',
    async (inlineElements) => {
      await mount(<SectionListParent inlineElements={inlineElements} />);
      expect(mockRenders.rows).toBeGreaterThan(0);
      expect(itemRenders).toBeGreaterThan(0);
      await expectStableRerender();
      await expectStableRerender();
    }
  );

  it('keeps rows alone with the device trace on', async () => {
    const lines: string[] = [];
    (
      globalThis as { __shadowlistTrace?: (line: string) => void }
    ).__shadowlistTrace = (line) => lines.push(line);
    try {
      await mount(<SectionListParent inlineElements />);
      await expectStableRerender();
    } finally {
      delete (globalThis as { __shadowlistTrace?: unknown }).__shadowlistTrace;
    }
    expect(lines.some((line) => line.startsWith('render id='))).toBe(true);
    expect(
      lines.filter((line) => line.startsWith('section-deps changed=')).length
    ).toBe(1);
    expect(lines.some((line) => line.startsWith('row-miss'))).toBe(false);
  });
});
