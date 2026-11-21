import { afterEach, expect, it, jest } from '@jest/globals';
import TestRenderer, { act, type ReactTestRenderer } from 'react-test-renderer';
import { plainSource, type RowSource } from '../virtualizer/rowSource';
import { useRowSpans } from '../virtualizer/useRowSpans';

let renderer: ReactTestRenderer | undefined;
let result: ReturnType<typeof useRowSpans>;
function Probe({
  source,
  provider,
  columns = 3,
  extraData,
}: {
  source: RowSource<number>;
  provider?: (item: number, index: number) => number;
  columns?: number;
  extraData?: unknown;
}) {
  result = useRowSpans(source, provider, columns, extraData);
  return null;
}
afterEach(async () => {
  await act(async () => renderer?.unmount());
});

it('snapshots all rows, normalizes values before codegen and keeps unchanged versions stable', async () => {
  const values = [NaN, Infinity, -4, 0, 2.9, 99];
  const source = plainSource(
    values,
    values.map((_, index) => String(index)),
    null,
    0
  );
  const provider = jest.fn((item: number) => item);
  await act(async () => {
    renderer = TestRenderer.create(
      <Probe source={source} provider={provider} />
    );
  });
  expect(result.rowSpans).toEqual([1, 1, 1, 1, 2, 3]);
  expect(provider).toHaveBeenCalledTimes(6);
  const first = result;
  await act(async () => {
    renderer!.update(<Probe source={source} provider={provider} />);
  });
  expect(provider).toHaveBeenCalledTimes(6);
  expect(result).toBe(first);
  await act(async () => {
    renderer!.update(
      <Probe source={source} provider={provider} extraData={1} />
    );
  });
  expect(provider).toHaveBeenCalledTimes(12);
  expect(result).toBe(first);
  await act(async () => {
    renderer!.update(<Probe source={source} provider={provider} columns={5} />);
  });
  expect(result.rowSpans).toEqual([1, 1, 1, 1, 2, 5]);
  expect(result.rowSpansVersion).toBeGreaterThan(first.rowSpansVersion);
});

it('re-evaluates index-dependent providers after key moves and clears on removal', async () => {
  const source = plainSource([10, 20], ['a', 'b'], null, 0);
  const calls: number[] = [];
  const provider = (item: number, index: number) => {
    calls.push(item);
    return index + 1;
  };
  await act(async () => {
    renderer = TestRenderer.create(
      <Probe source={source} provider={provider} />
    );
  });
  const first = result;
  const moved = plainSource([20, 10], ['b', 'a'], null, 0);
  await act(async () => {
    renderer!.update(<Probe source={moved} provider={provider} />);
  });
  expect(calls).toEqual([10, 20, 20, 10]);
  expect(result.rowSpans).toEqual([1, 2]);
  expect(result).toBe(first); // Column by index is unchanged; native metadata still checks key/index.
  await act(async () => {
    renderer!.update(<Probe source={moved} />);
  });
  expect(result.rowSpans).toEqual([]);
});
