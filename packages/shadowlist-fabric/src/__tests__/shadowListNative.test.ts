import { describe, expect, it } from '@jest/globals';
import {
  ELEMENT_MARKER,
  TEMPLATE_MARKER,
  encodeElementMarker,
  encodeTemplateMarker,
  toNativeStyle,
} from '../native/binding';

function decode(marker: string | undefined): unknown {
  expect(marker?.startsWith(ELEMENT_MARKER)).toBe(true);
  return JSON.parse(marker!.slice(ELEMENT_MARKER.length));
}

describe('encodeElementMarker', () => {
  it('leaves a plain element unmarked', () => {
    expect(encodeElementMarker({})).toBeUndefined();
    expect(encodeElementMarker({ bind: {} })).toBeUndefined();
  });

  it('carries the id, bindings and an action flag in the short form native parses', () => {
    expect(
      decode(
        encodeElementMarker({
          id: 'name',
          bind: { text: 'author.name', hidden: '!author.name' },
          action: 'open',
        })
      )
    ).toEqual({
      i: 'name',
      b: { text: 'author.name', hidden: '!author.name' },
      a: 1,
    });
  });

  it('does not send the action name to native, only that there is one', () => {
    expect(decode(encodeElementMarker({ action: 'like' }))).toEqual({ a: 1 });
  });

  it('marks a repeat with its path and cap', () => {
    expect(decode(encodeElementMarker({ repeat: 'images' }))).toEqual({
      r: 'images',
    });
    expect(
      decode(encodeElementMarker({ repeat: 'cards', repeatMax: 4 }))
    ).toEqual({ r: 'cards', m: 4 });
    // A cap without a repeat means nothing.
    expect(encodeElementMarker({ repeatMax: 4 })).toBeUndefined();
  });
});

describe('encodeTemplateMarker', () => {
  it('prefixes the template name', () => {
    expect(encodeTemplateMarker('post')).toBe(`${TEMPLATE_MARKER}post`);
  });
});

describe('toNativeStyle', () => {
  it('flattens style arrays and processes colors to numbers', () => {
    const style = toNativeStyle([
      { color: 'red', fontSize: 12 },
      { backgroundColor: '#00ff00', fontSize: 14 },
    ]);
    expect(style?.fontSize).toBe(14);
    expect(typeof style?.color).toBe('number');
    expect(typeof style?.backgroundColor).toBe('number');
  });

  it('returns null for no style, so native clears the override', () => {
    expect(toNativeStyle(null)).toBeNull();
    expect(toNativeStyle(undefined)).toBeNull();
  });

  it('drops undefined values and keeps explicit null colors', () => {
    expect(toNativeStyle({ opacity: undefined, color: null })).toEqual({
      color: null,
    });
  });
});
