import type { ComponentType } from 'react';
import { Text } from 'react-native';
import { afterEach, expect, it, jest } from '@jest/globals';
import TestRenderer, { act, type ReactTestRenderer } from 'react-test-renderer';
import SectionList from '../SectionList';
import { setShadowListStoreModule } from '../virtualizer/nativeStore';

type Props = Record<string, unknown>;
const mockRenders: Props[] = [];
jest.mock('../ShadowListViewNativeComponent', () => {
  const actual = jest.requireActual<
    typeof import('../ShadowListViewNativeComponent')
  >('../ShadowListViewNativeComponent');
  const Host = actual.default as unknown as ComponentType<Props>;
  function RecordingList(props: Props) {
    mockRenders.push(props);
    return <Host {...props} />;
  }
  return { __esModule: true, ...actual, default: RecordingList };
});
let renderer: ReactTestRenderer | undefined;
afterEach(async () => {
  await act(async () => renderer?.unmount());
  setShadowListStoreModule(null);
});

it('section grid edges span every column while callbacks receive section-local item indices', async () => {
  setShadowListStoreModule(null);
  const sections = [
    { key: 'a', data: [10, 20] },
    { key: 'empty', data: [] },
    { key: 'c', data: [30] },
  ];
  const calls: Array<[number, number, string]> = [];
  await act(async () => {
    renderer = TestRenderer.create(
      <SectionList
        sections={sections}
        numberOfColumns={3}
        renderItem={({ item }) => <Text>{item}</Text>}
        renderSectionHeader={() => <Text>Header</Text>}
        renderSectionFooter={() => null}
        getItemSpan={(item, index, section) => {
          calls.push([item, index, section.key!]);
          return index + 1;
        }}
      />
    );
  });
  expect(calls).toEqual([
    [10, 0, 'a'],
    [20, 1, 'a'],
    [30, 0, 'c'],
  ]);
  expect(mockRenders.at(-1)!.rowSpans).toEqual([3, 1, 2, 3, 3, 3, 3, 1, 3]);
});

it('ordinary single-column sections do not materialize a span snapshot', async () => {
  setShadowListStoreModule(null);
  await act(async () => {
    renderer = TestRenderer.create(
      <SectionList
        sections={[{ key: 'a', data: [1, 2] }]}
        renderItem={({ item }) => <Text>{item}</Text>}
        renderSectionHeader={() => <Text>Header</Text>}
      />
    );
  });
  expect(mockRenders.at(-1)!.rowSpans).toEqual([]);
});
