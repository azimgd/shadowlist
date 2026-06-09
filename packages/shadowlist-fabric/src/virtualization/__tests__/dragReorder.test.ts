import { describe, expect, it } from '@jest/globals';
import { arrayMove, unionDraggedIndex } from '../dragReorder';

describe('arrayMove', () => {
  const INPUT = ['a', 'b', 'c', 'd'];

  it('moves an item forward and backward', () => {
    expect(arrayMove(INPUT, 0, 2)).toEqual(['b', 'c', 'a', 'd']);
    expect(arrayMove(INPUT, 3, 1)).toEqual(['a', 'd', 'b', 'c']);
  });

  it('returns an untouched copy for no-op or out-of-bounds moves', () => {
    expect(arrayMove(INPUT, 1, 1)).toEqual(INPUT);
    expect(arrayMove(INPUT, -1, 2)).toEqual(INPUT);
    expect(arrayMove(INPUT, 0, 4)).toEqual(INPUT);
    expect(arrayMove(INPUT, 4, 0)).toEqual(INPUT);
  });

  it('never mutates the input', () => {
    const before = [...INPUT];
    arrayMove(INPUT, 0, 3);
    expect(INPUT).toEqual(before);
  });
});

describe('unionDraggedIndex', () => {
  const MOUNTED = [3, 4, 5];

  it('returns the input array identity when nothing needs adding', () => {
    expect(unionDraggedIndex(MOUNTED, -1, 10)).toBe(MOUNTED);
    expect(unionDraggedIndex(MOUNTED, 10, 10)).toBe(MOUNTED); // out of data
    expect(unionDraggedIndex(MOUNTED, 4, 10)).toBe(MOUNTED); // already mounted
  });

  it('inserts the dragged index keeping ascending order', () => {
    expect(unionDraggedIndex(MOUNTED, 1, 10)).toEqual([1, 3, 4, 5]);
    expect(unionDraggedIndex(MOUNTED, 8, 10)).toEqual([3, 4, 5, 8]);
  });
});
