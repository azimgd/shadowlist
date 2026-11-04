import { createRef, useEffect, useState } from 'react';
import { Pressable, Text, View } from 'react-native';
import assert from 'node:assert/strict';
import { test } from 'node:test';
import {
  SectionList,
  ShadowList,
  TreeList,
  type ShadowListCommands,
} from '../src';
import { lines, renderApp, wheelEvent } from './helpers';

const COLUMNS = 20;
const ROWS = 5;

function numbers(count: number): { id: string; label: string }[] {
  return Array.from({ length: count }, (_, index) => ({
    id: `k${index}`,
    label: `row ${index}`,
  }));
}

const keyOf = (item: { id: string }) => item.id;

/*
 * A row that keeps itself in mounted for as long as React has it.
 */
function MountedRow({
  id,
  label,
  mounted,
}: {
  id: string;
  label: string;
  mounted: Set<string>;
}) {
  useEffect(() => {
    mounted.add(id);
    return () => {
      mounted.delete(id);
    };
  }, [id, mounted]);
  return <Text>{label}</Text>;
}

/*
 * Renders count rows and counts how many are alive, which is what virtualization is.
 */
function counted(count: number) {
  const alive = new Set<string>();
  const seen = new Set<string>();
  const renderItem = ({ item }: { item: { id: string; label: string } }) => {
    alive.add(item.id);
    seen.add(item.id);
    return <Text>{item.label}</Text>;
  };
  return { data: numbers(count), renderItem, alive, seen };
}

test('rows paint in order inside the view and nothing past it', async () => {
  const { data, renderItem } = counted(50);
  const host = await renderApp(
    <ShadowList data={data} keyExtractor={keyOf} renderItem={renderItem} />,
    { columns: COLUMNS, rows: ROWS }
  );
  assert.deepEqual(lines(host), ['row 0', 'row 1', 'row 2', 'row 3', 'row 4']);
});

test('a list clips rows to its own frame', async () => {
  const { data, renderItem } = counted(50);
  const host = await renderApp(
    <View style={{ flex: 1 }}>
      <Text>top</Text>
      <View style={{ height: 2 }}>
        <ShadowList data={data} keyExtractor={keyOf} renderItem={renderItem} />
      </View>
      <Text>bottom</Text>
    </View>,
    { columns: COLUMNS, rows: ROWS }
  );
  assert.deepEqual(lines(host).slice(0, 4), [
    'top',
    'row 0',
    'row 1',
    'bottom',
  ]);
});

test('a 100,000 row list mounts only what is near the window', async () => {
  const data = numbers(100000);
  const mounted = new Set<string>();
  let renders = 0;
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      renderItem={({ item }) => {
        renders++;
        return <MountedRow id={item.id} label={item.label} mounted={mounted} />;
      }}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  assert.equal(lines(host)[0], 'row 0');
  // The initial rows, nothing more.
  assert.ok(renders <= 21, `rendered ${renders}`);
  // Then the window and its overscan of 4 each way, once the core has reported.
  assert.equal(mounted.size, ROWS + 4);
  assert.ok(mounted.has('k0'));
  assert.ok(!mounted.has('k9'));

  host.send(wheelEvent(30));
  await host.settle();
  assert.equal(lines(host)[0], 'row 30');
  assert.equal(mounted.size, ROWS + 8);
  assert.ok(mounted.has('k26') && mounted.has('k38'));
  assert.ok(!mounted.has('k0'));
});

test('the wheel scrolls by cells and reports the offset', async () => {
  const offsets: number[] = [];
  const { data, renderItem } = counted(100);
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      renderItem={renderItem}
      onScroll={(event) => offsets.push(event.nativeEvent.contentOffset.y)}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  host.send(wheelEvent(3));
  await host.settle();
  assert.equal(lines(host)[0], 'row 3');
  assert.deepEqual(offsets, [3]);
  host.send(wheelEvent(-100));
  await host.settle();
  assert.equal(lines(host)[0], 'row 0');
  assert.deepEqual(offsets, [3, 0]);
});

test('scrolling far mounts new rows and unmounts the old ones', async () => {
  const { data, renderItem, seen } = counted(1000);
  const host = await renderApp(
    <ShadowList data={data} keyExtractor={keyOf} renderItem={renderItem} />,
    { columns: COLUMNS, rows: ROWS }
  );
  for (let step = 0; step < 10; step++) {
    host.send(wheelEvent(3));
    await host.settle();
  }
  assert.deepEqual(lines(host).slice(0, 2), ['row 30', 'row 31']);
  assert.ok(seen.size < 60, `rendered ${seen.size} different rows`);
});

test('the end of the content is as far as the wheel goes', async () => {
  const { data, renderItem } = counted(8);
  const host = await renderApp(
    <ShadowList data={data} keyExtractor={keyOf} renderItem={renderItem} />,
    { columns: COLUMNS, rows: ROWS }
  );
  host.send(wheelEvent(100));
  await host.settle();
  assert.deepEqual(lines(host), ['row 3', 'row 4', 'row 5', 'row 6', 'row 7']);
});

test('rows taller than a cell take their real height', async () => {
  const data = numbers(10);
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      renderItem={({ item }) => (
        <View style={{ height: 2 }}>
          <Text>{item.label}</Text>
        </View>
      )}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  assert.deepEqual(lines(host).slice(0, 5), [
    'row 0',
    '',
    'row 1',
    '',
    'row 2',
  ]);
});

test('commands scroll to a row, an offset and the end', async () => {
  const ref = createRef<ShadowListCommands>();
  const data = numbers(100000);
  const host = await renderApp(
    <ShadowList
      ref={ref}
      data={data}
      keyExtractor={keyOf}
      renderItem={({ item }) => <Text>{item.label}</Text>}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  ref.current!.scrollToIndex({ index: 50000, animated: false });
  await host.settle();
  assert.equal(lines(host)[0], 'row 50000');

  ref.current!.scrollToIndex({ index: 60000, viewPosition: 1 });
  await host.settle();
  assert.equal(lines(host)[ROWS - 1], 'row 60000');

  ref.current!.scrollToItem({ item: data[70000]! });
  await host.settle();
  assert.equal(lines(host)[0], 'row 70000');

  ref.current!.scrollToOffset({ offset: 10 });
  await host.settle();
  assert.equal(lines(host)[0], 'row 10');

  ref.current!.scrollToEnd();
  await host.settle();
  assert.equal(lines(host)[ROWS - 1], 'row 99999');

  ref.current!.scrollToOffset({ offset: 0 });
  await host.settle();
  assert.equal(lines(host)[0], 'row 0');
});

test('a scroll command lands on its row among rows taller than a cell', async () => {
  const ref = createRef<ShadowListCommands>();
  const data = numbers(1000);
  const host = await renderApp(
    <ShadowList
      ref={ref}
      data={data}
      keyExtractor={keyOf}
      renderItem={({ item, index }) => (
        <View style={{ height: 1 + (index % 3) }}>
          <Text>{item.label}</Text>
        </View>
      )}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  for (const index of [500, 100, 900, 3]) {
    ref.current!.scrollToIndex({ index });
    await host.settle();
    assert.equal(lines(host)[0], `row ${index}`);
  }
  ref.current!.scrollToEnd();
  await host.settle();
  assert.ok(lines(host).some((line) => line === 'row 999'));
});

test('scrollToIndex out of range calls onScrollToIndexFailed', async () => {
  const ref = createRef<ShadowListCommands>();
  const failures: number[] = [];
  const data = numbers(10);
  const host = await renderApp(
    <ShadowList
      ref={ref}
      data={data}
      keyExtractor={keyOf}
      renderItem={({ item }) => <Text>{item.label}</Text>}
      onScrollToIndexFailed={(info) => failures.push(info.index)}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  ref.current!.scrollToIndex({ index: 99 });
  await host.settle();
  assert.deepEqual(failures, [99]);
});

test('selectItem, deselectItem and getSelectedIndices keep real selection', async () => {
  const ref = createRef<ShadowListCommands>();
  const changes: string[][] = [];
  const data = numbers(10);
  const host = await renderApp(
    <ShadowList
      ref={ref}
      data={data}
      keyExtractor={keyOf}
      allowsMultipleSelection
      onSelectionChange={(keys) => changes.push(keys)}
      renderItem={({ item, selected }) => (
        <Text>{`${selected ? '*' : ' '}${item.label}`}</Text>
      )}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  ref.current!.selectItem(2);
  ref.current!.selectItem(4);
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), [2, 4]);
  assert.equal(lines(host)[2], '*row 2');
  assert.equal(lines(host)[4], '*row 4');
  ref.current!.deselectItem(2);
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), [4]);
  assert.equal(lines(host)[2], ' row 2');
  assert.deepEqual(changes, [['k2'], ['k2', 'k4'], ['k4']]);
});

test('a row the data lost leaves the selection', async () => {
  const ref = createRef<ShadowListCommands>();
  let setData: (data: { id: string; label: string }[]) => void = () => {};
  function List() {
    const [data, set] = useState(numbers(5));
    setData = set;
    return (
      <ShadowList
        ref={ref}
        data={data}
        keyExtractor={keyOf}
        renderItem={({ item }) => <Text>{item.label}</Text>}
      />
    );
  }
  const host = await renderApp(<List />, { columns: COLUMNS, rows: ROWS });
  ref.current!.selectItem(1);
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), [1]);
  setData(numbers(5).filter((item) => item.id !== 'k1'));
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), []);
  assert.deepEqual(lines(host).slice(0, 2), ['row 0', 'row 2']);
});

test('the keyboard moves the selection and activates a row', async () => {
  const ref = createRef<ShadowListCommands>();
  const activated: number[] = [];
  const data = numbers(100);
  const host = await renderApp(
    <ShadowList
      ref={ref}
      data={data}
      keyExtractor={keyOf}
      enableSelectionOnKeyPress
      onActivateItem={({ index }) => activated.push(index)}
      renderItem={({ item, selected }) => (
        <Text>{`${selected ? '*' : ' '}${item.label}`}</Text>
      )}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  host.press('Tab');
  host.press('ArrowDown');
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), [0]);
  host.press('ArrowDown');
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), [1]);
  host.press('ArrowUp');
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), [0]);
  host.press('Enter');
  await host.settle();
  assert.deepEqual(activated, [0]);

  host.press('PageDown');
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), [ROWS]);
  assert.ok(lines(host).includes(`*row ${ROWS}`));

  host.press('End');
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), [99]);
  assert.equal(lines(host)[ROWS - 1], '*row 99');

  host.press('PageUp');
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), [99 - ROWS]);
  assert.ok(lines(host).includes(`*row ${99 - ROWS}`));

  host.press('Home');
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), [0]);
  assert.equal(lines(host)[0], '*row 0');
});

test('without enableSelectionOnKeyPress a list takes no keys', async () => {
  const ref = createRef<ShadowListCommands>();
  const data = numbers(10);
  const host = await renderApp(
    <ShadowList
      ref={ref}
      data={data}
      keyExtractor={keyOf}
      renderItem={({ item }) => <Text>{item.label}</Text>}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  host.press('Tab');
  host.press('ArrowDown');
  await host.settle();
  assert.deepEqual(ref.current!.getSelectedIndices(), []);
});

test('a press in a row reaches the row at its scrolled place', async () => {
  const pressed: string[] = [];
  const data = numbers(100);
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      renderItem={({ item }) => (
        <Pressable onPress={() => pressed.push(item.id)}>
          <Text>{item.label}</Text>
        </Pressable>
      )}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  host.send(wheelEvent(6));
  await host.settle();
  const click = {
    type: 'pointer',
    x: 1,
    y: 1,
    button: 'left',
    shiftKey: false,
    ctrlKey: false,
    altKey: false,
    metaKey: false,
  };
  host.send({ ...click, kind: 'down' });
  host.send({ ...click, kind: 'up' });
  await host.settle();
  assert.deepEqual(pressed, ['k7']);
});

test('measure in a row reads where the row is, not where layout left it', async () => {
  const data = numbers(20);
  const rowRefs = new Map<string, any>();
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      renderItem={({ item }) => (
        <View
          ref={(node) => {
            rowRefs.set(item.id, node);
          }}
        >
          <Text>{item.label}</Text>
        </View>
      )}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  host.send(wheelEvent(2));
  await host.settle();
  const pageY = await new Promise((resolve) => {
    rowRefs
      .get('k3')
      .measure(
        (
          _x: number,
          _y: number,
          _w: number,
          _h: number,
          _p: number,
          y: number
        ) => resolve(y)
      );
  });
  assert.equal(pageY, 1);
});

test('header, footer and empty templates sit around the rows', async () => {
  const { data, renderItem } = counted(2);
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      renderItem={renderItem}
      ListHeaderComponent={<Text>head</Text>}
      ListFooterComponent={<Text>foot</Text>}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  assert.deepEqual(lines(host).slice(0, 4), ['head', 'row 0', 'row 1', 'foot']);
  const empty = await renderApp(
    <ShadowList
      data={[]}
      keyExtractor={keyOf}
      renderItem={renderItem}
      ListHeaderComponent={<Text>head</Text>}
      ListEmptyComponent={<Text>nothing</Text>}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  assert.deepEqual(lines(empty).slice(0, 2), ['head', 'nothing']);
});

test('an inverted list opens at its end and follows what is appended', async () => {
  let setData: (data: { id: string; label: string }[]) => void = () => {};
  const ends: boolean[] = [];
  function List() {
    const [data, set] = useState(numbers(50));
    setData = set;
    return (
      <ShadowList
        data={data}
        keyExtractor={keyOf}
        inverted
        followAppends
        onAtEndChange={(atEnd) => ends.push(atEnd)}
        renderItem={({ item }) => <Text>{item.label}</Text>}
      />
    );
  }
  const host = await renderApp(<List />, { columns: COLUMNS, rows: ROWS });
  assert.equal(lines(host)[ROWS - 1], 'row 49');
  setData(numbers(51));
  await host.settle();
  assert.equal(lines(host)[ROWS - 1], 'row 50');

  host.send(wheelEvent(-3));
  await host.settle();
  assert.equal(lines(host)[ROWS - 1], 'row 47');
  assert.deepEqual(ends, [false]);
  setData(numbers(52));
  await host.settle();
  assert.equal(lines(host)[ROWS - 1], 'row 47');
});

test('getAnchorState and restoreAnchorState come back to a row', async () => {
  const ref = createRef<ShadowListCommands>();
  const data = numbers(100);
  const host = await renderApp(
    <ShadowList
      ref={ref}
      data={data}
      keyExtractor={keyOf}
      renderItem={({ item }) => <Text>{item.label}</Text>}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  host.send(wheelEvent(12));
  await host.settle();
  const anchor = await ref.current!.getAnchorState();
  assert.deepEqual(anchor, { key: 'k12', offset: 0 });
  host.send(wheelEvent(30));
  await host.settle();
  assert.equal(lines(host)[0], 'row 42');
  ref.current!.restoreAnchorState(anchor!);
  await host.settle();
  assert.equal(lines(host)[0], 'row 12');
});

test('edge callbacks and viewable items report', async () => {
  let ends = 0;
  const viewable: string[][] = [];
  const data = numbers(30);
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      onEndReached={() => ends++}
      onEndReachedThreshold={0}
      viewabilityConfig={{ itemVisiblePercentThreshold: 100 }}
      onViewableItemsChanged={({ viewableItems }) =>
        viewable.push(viewableItems.map((token) => token.key))
      }
      renderItem={({ item }) => <Text>{item.label}</Text>}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  assert.deepEqual(viewable.at(-1), ['k0', 'k1', 'k2', 'k3', 'k4']);
  assert.equal(ends, 0);
  host.send(wheelEvent(100));
  await host.settle();
  assert.equal(ends, 1);
  assert.deepEqual(viewable.at(-1), ['k25', 'k26', 'k27', 'k28', 'k29']);
});

test('a horizontal list lays out and scrolls along x', async () => {
  const data = numbers(50);
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      horizontal
      renderItem={({ item }) => (
        <View style={{ width: 4 }}>
          <Text>{item.id}</Text>
        </View>
      )}
    />,
    { columns: 12, rows: 2 }
  );
  assert.equal(lines(host)[0], 'k0  k1  k2');
  host.send(wheelEvent(0, 8));
  await host.settle();
  assert.equal(lines(host)[0], 'k2  k3  k4');
});

test('columns place rows side by side', async () => {
  const data = numbers(8);
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      numberOfColumns={2}
      renderItem={({ item }) => <Text>{item.id}</Text>}
    />,
    { columns: 10, rows: 3 }
  );
  assert.deepEqual(lines(host).slice(0, 2), ['k0   k1', 'k2   k3']);
});

test('a header can stay pinned while the rows scroll under it', async () => {
  const { data, renderItem } = counted(50);
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      renderItem={renderItem}
      stickyHeader
      ListHeaderComponent={
        <View style={{ backgroundColor: '#000000' }}>
          <Text>head</Text>
        </View>
      }
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  host.send(wheelEvent(10));
  await host.settle();
  assert.deepEqual(lines(host).slice(0, 2), ['head', 'row 10']);
});

test('prefetchDataSource hears the rows past the mounted ones', async () => {
  const prefetched: number[] = [];
  const { data, renderItem } = counted(200);
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      renderItem={renderItem}
      prefetchDataSource={{
        prefetchItems: (indices) => prefetched.push(...indices),
      }}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  host.send(wheelEvent(3));
  await host.settle();
  assert.ok(prefetched.length > 0);
  assert.ok(prefetched.every((index) => index > 5));
});

test('a new width keeps the row at the top of the window', async () => {
  const data = numbers(100);
  const host = await renderApp(
    <ShadowList
      data={data}
      keyExtractor={keyOf}
      renderItem={({ item }) => <Text>{`${item.label} is a long row`}</Text>}
    />,
    { columns: 40, rows: ROWS }
  );
  host.send(wheelEvent(20));
  await host.settle();
  assert.equal(lines(host)[0], 'row 20 is a long row');
  host.send({ type: 'resize', columns: 10, rows: ROWS });
  await host.settle();
  assert.deepEqual(lines(host).slice(0, 4), [
    'row 20 is',
    'a long row',
    'row 21 is',
    'a long row',
  ]);
});

test('SectionList renders sections through the native key store', async () => {
  const host = await renderApp(
    <SectionList
      sections={[
        { key: 'a', title: 'A', data: ['one', 'two'] },
        { key: 'b', title: 'B', data: ['three'] },
      ]}
      renderSectionHeader={({ section }) => <Text>{section.title}</Text>}
      renderItem={({ item }) => <Text>{item}</Text>}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  assert.deepEqual(lines(host).slice(0, 5), ['A', 'one', 'two', 'B', 'three']);
});

test('a sticky section header pins above the rows it heads', async () => {
  const host = await renderApp(
    <SectionList
      sections={[
        {
          key: 'a',
          title: 'A',
          data: Array.from({ length: 30 }, (_, index) => `a${index}`),
        },
        { key: 'b', title: 'B', data: ['b0', 'b1'] },
      ]}
      stickySectionHeadersEnabled
      renderSectionHeader={({ section }) => (
        <View style={{ backgroundColor: '#000000' }}>
          <Text>{section.title}</Text>
        </View>
      )}
      renderItem={({ item }) => <Text>{item}</Text>}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  host.send(wheelEvent(6));
  await host.settle();
  assert.equal(lines(host)[0], 'A');
  assert.equal(lines(host)[1], 'a6');
});

test('TreeList expands and collapses through the native key store', async () => {
  interface Node {
    key: string;
    label: string;
    children?: Node[];
  }
  const nodes: Node[] = [
    {
      key: 'root',
      label: 'root',
      children: [
        { key: 'leaf1', label: 'leaf1' },
        { key: 'leaf2', label: 'leaf2' },
      ],
    },
    { key: 'other', label: 'other' },
  ];
  let toggleRoot: () => void = () => {};
  const host = await renderApp(
    <TreeList
      data={nodes}
      keyExtractor={(node) => node.key}
      getChildren={(node) => node.children}
      renderItem={({ item, toggle }) => {
        if (item.key === 'root') toggleRoot = toggle;
        return <Text>{item.label}</Text>;
      }}
    />,
    { columns: COLUMNS, rows: ROWS }
  );
  assert.deepEqual(lines(host).slice(0, 2), ['root', 'other']);
  toggleRoot();
  await host.settle();
  assert.deepEqual(lines(host).slice(0, 4), [
    'root',
    'leaf1',
    'leaf2',
    'other',
  ]);
  toggleRoot();
  await host.settle();
  assert.deepEqual(lines(host).slice(0, 2), ['root', 'other']);
});
