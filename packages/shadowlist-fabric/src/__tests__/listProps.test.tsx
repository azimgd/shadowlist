import { afterEach, describe, expect, it, jest } from '@jest/globals';
import type { ComponentType, ReactElement } from 'react';
import { Platform, Text } from 'react-native';
import TestRenderer, { act, type ReactTestRenderer } from 'react-test-renderer';
import ShadowList from '../ShadowList';

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
