import { describe, expect, it } from '@jest/globals';
import { resolveRangeAnchorShift } from '../anchorShift';

interface Item {
  id: string;
}

const items = (ids: string[]): Item[] => ids.map((id) => ({ id }));
const keyOf = (item: Item) => item.id;

// Base list a..j (10 items).
const BASE = items(['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j']);

describe('resolveRangeAnchorShift', () => {
  it('shifts and unions on a prepend above the range', () => {
    const next = [...items(['x', 'y', 'z']), ...BASE];
    // Anchor 'c' (index 2) moved to index 5: delta +3, union keeps the old low.
    expect(
      resolveRangeAnchorShift(BASE, next, { low: 2, high: 6 }, keyOf)
    ).toEqual({ low: 2, high: 9 });
  });

  it('shifts and unions on a removal above the range', () => {
    const next = BASE.slice(2); // remove a, b
    // Anchor 'd' (index 3) moved to index 1: delta -2, union keeps the old high.
    expect(
      resolveRangeAnchorShift(BASE, next, { low: 3, high: 7 }, keyOf)
    ).toEqual({ low: 1, high: 7 });
  });

  it('returns null when the anchor index is unchanged (append / mid-range edit below)', () => {
    const next = [...BASE, ...items(['k', 'l'])];
    expect(
      resolveRangeAnchorShift(BASE, next, { low: 2, high: 6 }, keyOf)
    ).toBeNull();
  });

  it('returns null when the anchor row was removed (falls back to the native round-trip)', () => {
    const next = [...BASE.slice(0, 2), ...BASE.slice(3)]; // remove 'c'
    expect(
      resolveRangeAnchorShift(BASE, next, { low: 2, high: 6 }, keyOf)
    ).toBeNull();
  });

  it('returns null when nothing is mounted', () => {
    expect(
      resolveRangeAnchorShift(BASE, BASE, { low: -1, high: -1 }, keyOf)
    ).toBeNull();
  });

  it('returns null when the anchor index is beyond the previous data', () => {
    expect(
      resolveRangeAnchorShift(
        BASE.slice(0, 3),
        BASE,
        { low: 5, high: 8 },
        keyOf
      )
    ).toBeNull();
  });

  it('clamps the shifted range to the new data bounds', () => {
    // Keep only f..j and prepend one: 'f' moves 5 -> 1 (delta -4); the unioned
    // high (9) exceeds the new max index (5) and clamps.
    const next = [...items(['x']), ...BASE.slice(5)];
    expect(
      resolveRangeAnchorShift(BASE, next, { low: 5, high: 9 }, keyOf)
    ).toEqual({ low: 1, high: 5 });
  });

  it('handles a prepend while the full list is mounted', () => {
    const next = [...items(['x', 'y']), ...BASE];
    expect(
      resolveRangeAnchorShift(BASE, next, { low: 0, high: 9 }, keyOf)
    ).toEqual({ low: 0, high: 11 });
  });
});
