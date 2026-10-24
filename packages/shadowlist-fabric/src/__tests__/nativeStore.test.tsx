import type { ComponentType, ReactElement } from 'react';
import { Pressable, Text } from 'react-native';
import {
  afterEach,
  beforeEach,
  describe,
  expect,
  it,
  jest,
} from '@jest/globals';
import TestRenderer, { act, type ReactTestRenderer } from 'react-test-renderer';
import ShadowList from '../ShadowList';
import SectionList from '../SectionList';
import TreeList from '../TreeList';
import { setShadowListStoreModule } from '../virtualizer/nativeStore';
import {
  buildTreeSource,
  createTreeSourceState,
  type TreeFlatRow,
} from '../virtualizer/treeSource';
import { rowsOf } from '../virtualizer/sectionSource';
import { headerRowKey, itemRowKey } from '../virtualizer/sectionKeys';
import {
  createFakeStoreModule,
  type FakeStoreModule,
} from '../__fixtures__/shadowListStore';

/*
 * The lists with the native key store: keys, section rows, tree rows and size specs go to the
 * store by version, and the props only name the version.
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
  text?: string;
}

const MANY: Row[] = Array.from({ length: 200 }, (_, index) => ({
  id: `r${index}`,
}));

function keyOf(row: Row) {
  return row.id;
}

function renderRow({ item }: { item: Row }) {
  return <Text>{item.id}</Text>;
}

let renderer: ReactTestRenderer | null = null;
let module: FakeStoreModule;

async function mount(tree: ReactElement) {
  await act(async () => {
    renderer = TestRenderer.create(tree);
  });
}

async function update(tree: ReactElement) {
  await act(async () => {
    renderer!.update(tree);
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

function nativeKeys(): string[] {
  const props = listProps();
  return module.stores[0]!.versions.get(props.keysVersion as number)!.keys;
}

/*
 * Keys of the cells in the newest list render, in render order.
 */
function mountedCellKeys(): string[] {
  const rendered = renderer!.root.findAll(
    (node) => typeof node.props.rowKey === 'string' && node.props.rowIndex
  );
  return rendered.map((node) => node.props.rowKey as string);
}

beforeEach(() => {
  module = createFakeStoreModule();
  setShadowListStoreModule(module);
});

afterEach(async () => {
  await act(async () => {
    renderer?.unmount();
  });
  renderer = null;
  mockRenders.list = [];
  mockRenders.cells = [];
  setShadowListStoreModule(undefined);
});

describe('ShadowList with the native key store', () => {
  it('names a version of its keys instead of sending them, and sends only what changed', async () => {
    await mount(
      <ShadowList data={MANY} renderItem={renderRow} keyExtractor={keyOf} />
    );
    expect(listProps()).toMatchObject({
      keysStore: 1,
      keysVersion: 1,
      rowKeys: [],
    });
    expect(nativeKeys()).toEqual(MANY.map(keyOf));
    expect(module.calls).toContain('setKeys 1 200');

    const appended = [...MANY, { id: 'new' }];
    await update(
      <ShadowList data={appended} renderItem={renderRow} keyExtractor={keyOf} />
    );
    expect(module.calls).toContain('spliceKeys 2 1 200 0 1');
    expect(listProps().keysVersion).toBe(2);
    expect(nativeKeys()).toEqual(appended.map(keyOf));

    // An item edited in place keeps the version: native gets no new keys.
    const edited = appended.slice();
    edited[5] = { id: 'r5', text: 'edited' };
    const writes = module.calls.length;
    await update(
      <ShadowList data={edited} renderItem={renderRow} keyExtractor={keyOf} />
    );
    expect(listProps().keysVersion).toBe(2);
    expect(
      module.calls
        .slice(writes)
        .filter((call) => !call.startsWith('indexOfKey'))
    ).toEqual([]);

    // A row replaced in the middle sends one key.
    const replaced = edited.slice();
    replaced[100] = { id: 'swap' };
    await update(
      <ShadowList data={replaced} renderItem={renderRow} keyExtractor={keyOf} />
    );
    expect(module.calls).toContain('spliceKeys 3 2 100 1 1');
    expect(nativeKeys()).toEqual(replaced.map(keyOf));
  });

  it('finds the mounted range by the indices native sends, also after rows are prepended', async () => {
    await mount(
      <ShadowList data={MANY} renderItem={renderRow} keyExtractor={keyOf} />
    );
    await sendToList('onMountedRangeChange', {
      lowKey: 'r30',
      highKey: 'r45',
      lowIndex: 30,
      highIndex: 45,
      lowAtStart: false,
      highAtEnd: false,
      sequence: 1,
      generation: 0,
      landed: false,
    });
    expect(listProps()).toMatchObject({ mountedLow: 30, mountedHigh: 45 });
    const before = module.calls.length;
    const prepended = [{ id: 'p0' }, { id: 'p1' }, ...MANY];
    await update(
      <ShadowList
        data={prepended}
        renderItem={renderRow}
        keyExtractor={keyOf}
      />
    );
    expect(listProps()).toMatchObject({ mountedLow: 32, mountedHigh: 47 });
    // Found near the hints, without asking native.
    expect(
      module.calls.slice(before).some((call) => call.startsWith('indexOfKey'))
    ).toBe(false);
    const keys = mountedCellKeys();
    expect(keys[0]).toBe('r30');
    expect(keys[keys.length - 1]).toBe('r45');
  });

  it('sends row size specs as objects in the store, not as JSON', async () => {
    const data = MANY.map((row) => ({ ...row, text: `text ${row.id}` }));
    await mount(
      <ShadowList
        data={data}
        renderItem={renderRow}
        keyExtractor={keyOf}
        getItemSizeSpec={(item) => ({ text: item.text!, fontSize: 15 })}
      />
    );
    const props = listProps();
    expect(props.rowSizeSpecs).toBe('');
    const version = props.sizeSpecsVersion as number;
    expect(version).toBeGreaterThan(0);
    const written = module.stores[0]!.specs.get(version)!;
    expect(written.keys[0]).toBe('r0');
    expect(written.specs[0]).toEqual({ text: 'text r0', fontSize: 15 });
  });
});

describe('SectionList with the native key store', () => {
  it('lets native build the row keys and pin the headers', async () => {
    const sections = [
      { key: 'a', data: [{ id: '0' }, { id: '1' }] },
      { key: 'b', data: [{ id: '0' }] },
    ];
    await mount(
      <SectionList
        sections={sections}
        renderItem={renderRow}
        renderSectionHeader={({ section }) => <Text>{section.key}</Text>}
      />
    );
    expect(listProps()).toMatchObject({ rowKeys: [], stickyIndices: [] });
    expect(nativeKeys()).toEqual([
      headerRowKey('a'),
      itemRowKey('a', '0'),
      itemRowKey('a', '1'),
      headerRowKey('b'),
      itemRowKey('b', '0'),
    ]);
    expect(module.stores[0]!.versions.get(1)!.sticky).toEqual([0, 3]);
    // The pinned header overlay renders the section native picks.
    await sendToList('onViewabilityChange', {
      activeStickyIndex: 3,
      visibleLow: 3,
      visibleHigh: 4,
      changes: [],
    });
    const overlay = renderer!.root.findAll(
      (node) =>
        node.props.templateType === 'sectionHeader' &&
        typeof node.type !== 'string'
    );
    expect(overlay.length).toBeGreaterThan(0);
    expect(
      overlay[0]!.findAll(
        (node) => node.type === Text && node.props.children === 'b'
      ).length
    ).toBe(1);
  });
});

interface Node {
  id: string;
  children?: Node[];
}

function sampleTree(): Node[] {
  return [
    {
      id: 'a',
      children: [{ id: 'a.0', children: [{ id: 'a.0.0' }] }, { id: 'a.1' }],
    },
    { id: 'b', children: [{ id: 'b.0' }] },
  ];
}

describe('TreeList with the native key store', () => {
  it('splices only the toggled node in native', async () => {
    const data = sampleTree();
    const toggles: Record<string, () => void> = {};
    await mount(
      <TreeList<Node>
        data={data}
        getChildren={(node) => node.children}
        keyExtractor={(node) => node.id}
        initialExpandedKeys={['a']}
        renderItem={({ item, toggle }) => {
          toggles[item.id] = toggle;
          return (
            <Pressable onPress={toggle}>
              <Text>{item.id}</Text>
            </Pressable>
          );
        }}
      />
    );
    expect(nativeKeys()).toEqual(['a', 'a.0', 'a.1', 'b']);
    await act(async () => toggles['a.0']!());
    expect(module.calls).toContain('expandTree 2 1 1 1');
    expect(nativeKeys()).toEqual(['a', 'a.0', 'a.0.0', 'a.1', 'b']);
    await act(async () => toggles.a!());
    expect(module.calls).toContain('collapseTree 3 2 0');
    expect(nativeKeys()).toEqual(['a', 'b']);
    expect(
      module.calls.filter((call) => call.startsWith('resetTree'))
    ).toHaveLength(1);
  });
});

describe('tree sources', () => {
  /*
   * The flatten TreeList ran in JS before: depth first, children of expanded nodes only.
   */
  function referenceRows(
    nodes: Node[],
    expanded: ReadonlySet<string>,
    depth = 0,
    rows: {
      id: string;
      depth: number;
      hasChildren: boolean;
      isExpanded: boolean;
    }[] = []
  ) {
    for (const node of nodes) {
      const hasChildren = !!node.children && node.children.length > 0;
      const isExpanded = hasChildren && expanded.has(node.id);
      rows.push({ id: node.id, depth, hasChildren, isExpanded });
      if (isExpanded) referenceRows(node.children!, expanded, depth + 1, rows);
    }
    return rows;
  }

  function build(level: number, prefix: string): Node[] {
    return Array.from({ length: 3 }, (_, index) => {
      const id = `${prefix}${index}`;
      return level < 3 ? { id, children: build(level + 1, `${id}.`) } : { id };
    });
  }

  for (const withStore of [false, true]) {
    it(`match a full flatten through random toggles ${withStore ? 'with' : 'without'} the native store`, () => {
      const data = build(0, 'n');
      const getChildren = (node: Node) => node.children;
      const keyExtractor = (node: Node) => node.id;
      const fake = createFakeStoreModule();
      const handle = fake.createStore();
      const store = withStore
        ? { module: fake, handle, id: handle.id, version: 0 }
        : null;
      const state = createTreeSourceState<Node>();
      let expanded = new Set<string>();
      let seed = 5;
      let previousRows: TreeFlatRow<Node>[] = [];
      for (let step = 0; step < 200; step++) {
        const source = buildTreeSource(
          state,
          data,
          getChildren,
          keyExtractor,
          expanded,
          store
        );
        const rows = rowsOf(source);
        expect(
          rows.map(({ id, depth, hasChildren, isExpanded }) => ({
            id,
            depth,
            hasChildren,
            isExpanded,
          }))
        ).toEqual(referenceRows(data, expanded));
        if (withStore) {
          expect(
            fake.stores[0]!.versions.get(source.keysVersion)!.keys
          ).toEqual(rows.map((row) => row.id));
        } else {
          expect(source.propKeys).toEqual(rows.map((row) => row.id));
        }
        // Unchanged rows keep their objects.
        const before = new Map(previousRows.map((row) => [row.id, row]));
        for (const row of rows) {
          const old = before.get(row.id);
          if (old && old.isExpanded === row.isExpanded) expect(row).toBe(old);
        }
        previousRows = rows;
        // Toggle one to three visible nodes.
        const next = new Set(expanded);
        const toggles = 1 + (step % 3);
        for (let toggle = 0; toggle < toggles; toggle++) {
          seed = (seed * 1103515245 + 12345) % 2147483648;
          const row = rows[seed % rows.length]!;
          if (next.has(row.id)) next.delete(row.id);
          else next.add(row.id);
        }
        expanded = next;
      }
      if (withStore) {
        expect(
          fake.calls.filter((call) => call.startsWith('resetTree'))
        ).toHaveLength(1);
      }
    });
  }
});
