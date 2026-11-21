import { afterEach, expect, it } from '@jest/globals';
import TestRenderer, { act, type ReactTestRenderer } from 'react-test-renderer';
import type { ViewabilityChangeEvent } from '../ShadowListViewNativeComponent';
import { plainSource, type RowSource } from '../virtualizer/rowSource';
import { useViewability } from '../virtualizer/useViewability';

let renderer: ReactTestRenderer | undefined;
let result: ReturnType<typeof useViewability<number>>;
function Probe({
  source,
  version = 1,
  columns = 3,
}: {
  source: RowSource<number>;
  version?: number;
  columns?: number;
}) {
  result = useViewability({
    source,
    pairs: [],
    rowSpansVersion: version,
    numberOfColumns: columns,
  });
  return null;
}
async function report(
  rows: NonNullable<ViewabilityChangeEvent['rowColumns']>,
  version = 1,
  columns = 3
) {
  await act(async () =>
    result.handleViewabilityChange({
      nativeEvent: {
        activeStickyIndex: -1,
        visibleLow: -1,
        visibleHigh: -1,
        changes: [],
        rowColumns: rows,
        rowSpansVersion: version,
        numberOfColumns: columns,
      },
    } as unknown as Parameters<typeof result.handleViewabilityChange>[0])
  );
}
afterEach(async () => act(async () => renderer?.unmount()));

it('rejects cached ownership after a reorder before fresh geometry arrives', async () => {
  const source = plainSource([10, 20], ['a', 'b'], null, 0);
  await act(async () => {
    renderer = TestRenderer.create(<Probe source={source} />);
  });
  await report([
    { key: 'a', index: 0, column: 0 },
    { key: 'b', index: 1, column: 2 },
  ]);
  expect(result.getRowColumn('b', 1)).toBe(2);
  const moved = plainSource([20, 10], ['b', 'a'], null, 0);
  await act(async () => renderer!.update(<Probe source={moved} />));
  expect(result.getRowColumn('b', 0)).toBeUndefined();
  expect(result.getRowColumn('a', 1)).toBeUndefined();
  await report([{ key: 'b', index: 1, column: 2 }]);
  expect(result.getRowColumn('b', 0)).toBeUndefined();
  await report([
    { key: 'b', index: 0, column: 0 },
    { key: 'a', index: 1, column: 2 },
  ]);
  expect(result.getRowColumn('a', 1)).toBe(2);
});

it('retains same-key payload facts but rejects stale span and column-count stamps', async () => {
  const source = plainSource([10], ['a'], null, 0);
  await act(async () => {
    renderer = TestRenderer.create(<Probe source={source} />);
  });
  await report([{ key: 'a', index: 0, column: 1 }]);
  await act(async () =>
    renderer!.update(<Probe source={plainSource([1000], ['a'], null, 0)} />)
  );
  expect(result.getRowColumn('a', 0)).toBe(1);
  await act(async () =>
    renderer!.update(<Probe source={source} version={2} />)
  );
  expect(result.getRowColumn('a', 0)).toBeUndefined();
  await report([{ key: 'a', index: 0, column: 0 }]);
  expect(result.getRowColumn('a', 0)).toBeUndefined();
  await report([{ key: 'a', index: 0, column: 2 }], 2);
  expect(result.getRowColumn('a', 0)).toBe(2);
  await act(async () =>
    renderer!.update(<Probe source={source} version={2} columns={2} />)
  );
  expect(result.getRowColumn('a', 0)).toBeUndefined();
  await report(
    [
      { key: 'a', index: 0, column: 2 },
      { key: 'missing', index: -1, column: 0 },
    ],
    2,
    2
  );
  expect(result.getRowColumn('a', 0)).toBeUndefined();
});

it('accepts a real empty-layout invalidation and does not carry facts into a new list lifetime', async () => {
  const source = plainSource([10], ['a'], null, 0);
  await act(async () => {
    renderer = TestRenderer.create(<Probe source={source} />);
  });
  await report([{ key: 'a', index: 0, column: 2 }]);
  expect(result.getRowColumn('a', 0)).toBe(2);
  await report([]);
  expect(result.getRowColumn('a', 0)).toBeUndefined();
  const previousHandler = result.handleViewabilityChange;
  await act(async () => renderer!.unmount());
  await act(async () => {
    renderer = TestRenderer.create(<Probe source={source} />);
  });
  await act(async () =>
    previousHandler({
      nativeEvent: {
        activeStickyIndex: -1,
        visibleLow: -1,
        visibleHigh: -1,
        changes: [],
        rowSpansVersion: 1,
        numberOfColumns: 3,
        rowColumns: [{ key: 'a', index: 0, column: 2 }],
      },
    } as unknown as Parameters<typeof previousHandler>[0])
  );
  expect(result.getRowColumn('a', 0)).toBeUndefined();
  await report([{ key: 'a', index: 0, column: 0 }]);
  expect(result.getRowColumn('a', 0)).toBe(0);
});
