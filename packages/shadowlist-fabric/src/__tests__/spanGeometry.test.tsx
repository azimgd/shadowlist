import { StyleSheet, Text } from 'react-native';
import { afterEach, expect, it } from '@jest/globals';
import TestRenderer, { act, type ReactTestRenderer } from 'react-test-renderer';
import ShadowList from '../ShadowList';
import ShadowListView from '../ShadowListViewNativeComponent';
import ShadowListCellView from '../ShadowListCellViewNativeComponent';
import { setShadowListStoreModule } from '../virtualizer/nativeStore';

let renderer: ReactTestRenderer | undefined;
afterEach(async () => {
  await act(async () => renderer?.unmount());
  setShadowListStoreModule(null);
});

for (const horizontal of [false, true]) {
  it(`uses canonical edge padding and span size on the ${horizontal ? 'horizontal' : 'vertical'} cross axis`, async () => {
    setShadowListStoreModule(null);
    await act(async () => {
      renderer = TestRenderer.create(
        <ShadowList
          data={['a', 'b', 'c']}
          keyExtractor={(key) => key}
          horizontal={horizontal}
          numberOfColumns={3}
          getItemSpan={(_, index) => (index === 0 ? 2 : 1)}
          contentContainerStyle={
            horizontal
              ? { paddingTop: 12, paddingBottom: 18 }
              : { paddingLeft: 12, paddingRight: 18 }
          }
          columnWrapperStyle={{ columnGap: 6 }}
          renderItem={({ item }) => <Text>{item}</Text>}
        />
      );
    });
    const host = () => renderer!.root.findByType(ShadowListView);
    expect(host().props.rowColumnsEventEnabled).toBe(true);
    expect(host().props.columnPaddings).toEqual([12, 2, 4, 10, -4, 18]);
    await act(async () =>
      host().props.onViewabilityChange({
        nativeEvent: {
          activeStickyIndex: -1,
          visibleLow: -1,
          visibleHigh: -1,
          changes: [],
          rowSpansVersion: host().props.rowSpansVersion,
          numberOfColumns: 3,
          rowColumns: [
            { key: 'a', index: 0, column: 0 },
            { key: 'b', index: 1, column: 2 },
            { key: 'c', index: 2, column: 0 },
          ],
        },
      })
    );
    const cells = renderer!.root.findAllByType(ShadowListCellView);
    const wide = StyleSheet.flatten(
      cells.find((cell) => cell.props.rowKey === 'a')!.props.style
    );
    const last = StyleSheet.flatten(
      cells.find((cell) => cell.props.rowKey === 'b')!.props.style
    );
    expect(wide[horizontal ? 'height' : 'width']).toBe(`${200 / 3}%`);
    expect(wide[horizontal ? 'paddingTop' : 'paddingLeft']).toBe(12);
    expect(wide[horizontal ? 'paddingBottom' : 'paddingRight']).toBe(10);
    expect(last[horizontal ? 'paddingTop' : 'paddingLeft']).toBe(-4);
    expect(last[horizontal ? 'paddingBottom' : 'paddingRight']).toBe(18);
  });
}

it('does not request column metadata for ordinary equal-width rows', async () => {
  setShadowListStoreModule(null);
  await act(async () => {
    renderer = TestRenderer.create(
      <ShadowList
        data={[1, 2]}
        numberOfColumns={3}
        contentContainerStyle={{ padding: 12 }}
        renderItem={({ item }) => <Text>{item}</Text>}
      />
    );
  });
  expect(
    renderer!.root.findByType(ShadowListView).props.rowColumnsEventEnabled
  ).toBe(false);
});

it('updates covered edges with the same mounted band and ignores the previous provider stamp', async () => {
  setShadowListStoreModule(null);
  const data = ['a', 'b', 'c'];
  const render = (spans: number[]) => (
    <ShadowList
      data={data}
      keyExtractor={(key) => key}
      numberOfColumns={3}
      getItemSpan={(_, index) => spans[index]!}
      contentContainerStyle={{ paddingLeft: 12, paddingRight: 18 }}
      columnWrapperStyle={{ columnGap: 6 }}
      renderItem={({ item }) => <Text>{item}</Text>}
    />
  );
  await act(async () => {
    renderer = TestRenderer.create(render([2, 1, 1]));
  });
  const host = () => renderer!.root.findByType(ShadowListView);
  const previous = host().props;
  const report = async (
    version: number,
    rows: Array<{ key: string; index: number; column: number }>
  ) => {
    await act(async () =>
      host().props.onViewabilityChange({
        nativeEvent: {
          activeStickyIndex: -1,
          visibleLow: -1,
          visibleHigh: -1,
          changes: [],
          rowSpansVersion: version,
          numberOfColumns: 3,
          rowColumns: rows,
        },
      })
    );
  };
  const style = (key: string) =>
    StyleSheet.flatten(
      renderer!.root
        .findAllByType(ShadowListCellView)
        .find((cell) => cell.props.rowKey === key)!.props.style
    );
  await report(previous.rowSpansVersion, [{ key: 'b', index: 1, column: 2 }]);
  expect(style('b').paddingLeft).toBe(-4);
  await act(async () => renderer!.update(render([3, 1, 1])));
  expect([host().props.mountedLow, host().props.mountedHigh]).toEqual([
    previous.mountedLow,
    previous.mountedHigh,
  ]);
  expect(style('a')).toMatchObject({
    width: '100%',
    paddingLeft: 12,
    paddingRight: 18,
  });
  await report(previous.rowSpansVersion, [{ key: 'b', index: 1, column: 2 }]);
  expect(style('b').paddingLeft).toBe(12);
  await act(async () => renderer!.update(render([1, 2, 1])));
  await report(host().props.rowSpansVersion, [
    { key: 'b', index: 1, column: 1 },
  ]);
  expect(style('b')).toMatchObject({
    width: `${200 / 3}%`,
    paddingLeft: 4,
    paddingRight: 18,
  });
});
