import { describe, expect, it } from '@jest/globals';
import { diffViewableItems, resolveActiveStickyIndex } from '../viewableItems';
import type { ViewToken } from '../../types';

interface Item {
  id: string;
}

const DATA: Item[] = Array.from({ length: 10 }, (_, i) => ({ id: `k${i}` }));
const keyOf = (item: Item) => item.id;

const token = (index: number, isViewable = true): ViewToken<Item> => ({
  item: DATA[index]!,
  index,
  key: `k${index}`,
  isViewable,
});

describe('diffViewableItems', () => {
  it('reports everything as entered on the first emit', () => {
    const { viewableItems, changed } = diffViewableItems(DATA, 2, 4, [], keyOf);
    expect(viewableItems).toEqual([token(2), token(3), token(4)]);
    expect(changed).toEqual([token(2), token(3), token(4)]);
  });

  it('reports incremental enters and exits on a scroll', () => {
    const prev = [token(2), token(3), token(4)];
    const { viewableItems, changed } = diffViewableItems(
      DATA,
      3,
      5,
      prev,
      keyOf
    );
    expect(viewableItems).toEqual([token(3), token(4), token(5)]);
    expect(changed).toEqual([token(5), token(2, false)]);
  });

  it('reports nothing changed for an identical window', () => {
    const prev = [token(2), token(3)];
    const { changed } = diffViewableItems(DATA, 2, 3, prev, keyOf);
    expect(changed).toEqual([]);
  });

  it('normalizes an inverted window (start > end)', () => {
    const { viewableItems } = diffViewableItems(DATA, 4, 2, [], keyOf);
    expect(viewableItems).toEqual([token(2), token(3), token(4)]);
  });

  it('resolves an uninitialized window to everything leaving', () => {
    const prev = [token(2), token(3)];
    const { viewableItems, changed } = diffViewableItems(
      DATA,
      -1,
      -1,
      prev,
      keyOf
    );
    expect(viewableItems).toEqual([]);
    expect(changed).toEqual([token(2, false), token(3, false)]);
  });

  it('skips indices with no backing item', () => {
    const { viewableItems } = diffViewableItems(DATA, 8, 12, [], keyOf);
    expect(viewableItems).toEqual([token(8), token(9)]);
  });

  it('diffs by key, so a re-indexed item does not re-enter', () => {
    // The same keys now sit one index lower (a removal above the window).
    const prev = [token(3), token(4)];
    const shifted = DATA.slice(1);
    const { changed } = diffViewableItems(shifted, 2, 3, prev, keyOf);
    expect(changed).toEqual([]);
  });
});

describe('resolveActiveStickyIndex', () => {
  const HEADERS = [0, 5, 12];

  it('returns -1 with no sticky indices', () => {
    expect(resolveActiveStickyIndex(undefined, 3)).toBe(-1);
    expect(resolveActiveStickyIndex([], 3)).toBe(-1);
  });

  it('returns -1 while scrolled above the first header', () => {
    expect(resolveActiveStickyIndex([4, 9], 2)).toBe(-1);
  });

  it('activates a header exactly at the window top', () => {
    expect(resolveActiveStickyIndex(HEADERS, 5)).toBe(5);
  });

  it('keeps the previous header active between headers', () => {
    expect(resolveActiveStickyIndex(HEADERS, 4)).toBe(0);
    expect(resolveActiveStickyIndex(HEADERS, 11)).toBe(5);
  });

  it('keeps the last header active past the end', () => {
    expect(resolveActiveStickyIndex(HEADERS, 40)).toBe(12);
  });
});
