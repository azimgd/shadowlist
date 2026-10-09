import { describe, expect, it } from '@jest/globals';
import { View } from 'react-native';
import { defaultKeyExtractor } from '../virtualizer/helpers';
import { flattenSections } from '../virtualizer/sectionRows';
import type { SectionListProps, ShadowListProps } from '../types';

describe('defaultKeyExtractor', () => {
  it('reads a string id', () => {
    expect(defaultKeyExtractor({ id: 'a' }, 3)).toBe('a');
  });

  it('reads a number id as a string', () => {
    expect(defaultKeyExtractor({ id: 7 }, 3)).toBe('7');
  });

  it('falls back to the index without a usable id', () => {
    expect(defaultKeyExtractor({ title: 'x' }, 3)).toBe('3');
    expect(defaultKeyExtractor({ id: null }, 4)).toBe('4');
    expect(defaultKeyExtractor({ id: { nested: true } }, 5)).toBe('5');
    expect(defaultKeyExtractor('plain string', 6)).toBe('6');
    expect(defaultKeyExtractor(42, 7)).toBe('7');
    expect(defaultKeyExtractor(null, 8)).toBe('8');
    expect(defaultKeyExtractor(undefined, 9)).toBe('9');
  });
});

describe('section rows without a keyExtractor', () => {
  it('key elements by id and fall back to the index', () => {
    const data: ReadonlyArray<object> = [{ id: 'a' }, { title: 'b' }];
    const { rows } = flattenSections(
      [{ key: 's', data }],
      undefined,
      false,
      false,
      false,
      new Map()
    );
    expect(rows.map((row) => row.id)).toEqual(['si:s:a', 'si:s:1']);
  });
});

/*
 * Compile time checks. tsc runs over the tests, and these assignments fail the typecheck
 * if the list types start to require an id again.
 */
describe('element types', () => {
  it('accept elements without an id', () => {
    type Message = { uuid: string; text: string };
    const withKeys: ShadowListProps<Message> = {
      data: [{ uuid: 'u1', text: 'hi' }],
      renderElement: () => <View />,
      keyExtractor: (message) => message.uuid,
    };
    const withoutKeys: ShadowListProps<string> = {
      data: ['a', 'b'],
      renderElement: ({ element }) => <View testID={element} />,
    };
    const sections: SectionListProps<Message> = {
      sections: [{ data: [{ uuid: 'u2', text: 'yo' }] }],
      keyExtractor: (message) => message.uuid,
    };
    expect(withKeys.data).toHaveLength(1);
    expect(withoutKeys.data).toHaveLength(2);
    expect(sections.sections).toHaveLength(1);
  });
});
