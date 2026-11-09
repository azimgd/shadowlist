import { Text } from 'react-native';
import { afterEach, describe, expect, it } from '@jest/globals';
import TestRenderer, { act, type ReactTestRenderer } from 'react-test-renderer';
import ShadowList from '../ShadowList';

type Item = number | boolean | string | null;

const DATA: Item[] = [0, false, '', null, 1];

let renderer: ReactTestRenderer | null = null;

afterEach(async () => {
  await act(async () => {
    renderer?.unmount();
  });
  renderer = null;
});

describe('falsy items', () => {
  it('render a row for 0, false, an empty string and null', async () => {
    const rendered: Item[] = [];
    await act(async () => {
      renderer = TestRenderer.create(
        <ShadowList
          data={DATA}
          keyExtractor={(_item, index) => `row-${index}`}
          renderItem={({ item }) => {
            rendered.push(item);
            return <Text>{String(item)}</Text>;
          }}
        />
      );
    });
    expect(new Set(rendered)).toEqual(new Set(DATA));
    const texts = renderer!.root
      .findAllByType(Text)
      .map((node) => node.props.children);
    expect(texts).toEqual(['0', 'false', '', 'null', '1']);
  });
});
