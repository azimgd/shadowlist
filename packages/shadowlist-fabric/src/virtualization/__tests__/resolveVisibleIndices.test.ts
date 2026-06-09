import { describe, expect, it } from '@jest/globals';
import {
  resolveVisibleIndices,
  type ResolveVisibleIndicesInput,
} from '../resolveVisibleIndices';
import { initialMountedRange, rangeToIndices } from '../mountedRange';

/*
 * Fixture geometry used throughout: a 5-row visible range in a 100-row list.
 * leading overscan = ceil(5 * 1.5) = 8, trailing = ceil(5 * 0.5) = 3,
 * extend threshold (half the leading) = 4, max trail = 3 + 5 = 8.
 * Forward band for visible range [10..14] = [7..22]; backward band = [2..17].
 */
const NONE = { low: -1, high: -1 };

const input = (
  overrides: Partial<ResolveVisibleIndicesInput>
): ResolveVisibleIndicesInput => ({
  visibleStartIndex: 10,
  visibleEndIndex: 14,
  prevVisibleRange: NONE,
  mountedRange: NONE,
  dataLength: 100,
  ...overrides,
});

describe('ignore', () => {
  it('ignores an uninitialized start index', () => {
    expect(resolveVisibleIndices(input({ visibleStartIndex: -1 }))).toEqual({
      action: 'ignore',
    });
  });

  it('ignores an uninitialized end index', () => {
    expect(resolveVisibleIndices(input({ visibleEndIndex: -1 }))).toEqual({
      action: 'ignore',
    });
  });

  it('ignores events for an empty list', () => {
    expect(resolveVisibleIndices(input({ dataLength: 0 }))).toEqual({
      action: 'ignore',
    });
  });
});

describe('visible range normalization', () => {
  it('normalizes an inverted visible range (start > end) to ascending', () => {
    const inverted = resolveVisibleIndices(
      input({ visibleStartIndex: 14, visibleEndIndex: 10 })
    );
    const ascending = resolveVisibleIndices(
      input({ visibleStartIndex: 10, visibleEndIndex: 14 })
    );
    expect(inverted).toEqual(ascending);
    expect(inverted).toMatchObject({ visibleRange: { low: 10, high: 14 } });
  });
});

describe('mount (visible range left the mounted rows)', () => {
  it('mounts the band when nothing is mounted yet', () => {
    const result = resolveVisibleIndices(input({ mountedRange: NONE }));
    expect(result).toEqual({
      action: 'mount',
      visibleRange: { low: 10, high: 14 },
      band: { low: 7, high: 22 },
    });
  });

  it('mounts the band at the target on a disjoint jump (fling / scrollToIndex)', () => {
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 0, high: 12 },
        visibleStartIndex: 50,
        visibleEndIndex: 54,
        prevVisibleRange: { low: 10, high: 14 },
      })
    );
    expect(result).toEqual({
      action: 'mount',
      visibleRange: { low: 50, high: 54 },
      band: { low: 47, high: 62 },
    });
  });

  it('mounts the band on an adjacent overflow beyond the range', () => {
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 0, high: 12 },
        visibleStartIndex: 10,
        visibleEndIndex: 15,
      })
    );
    // span 6: leading = 9, trailing = 3.
    expect(result).toEqual({
      action: 'mount',
      visibleRange: { low: 10, high: 15 },
      band: { low: 7, high: 24 },
    });
  });

  it('clamps the band to the list bounds', () => {
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 0, high: 5 },
        visibleStartIndex: 95,
        visibleEndIndex: 99,
      })
    );
    expect(result).toMatchObject({
      action: 'mount',
      band: { low: 92, high: 99 },
    });
  });
});

describe('skip (hysteresis holds)', () => {
  it('skips while enough lead is mounted ahead and the trail is short', () => {
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 7, high: 22 },
        prevVisibleRange: { low: 9, high: 13 },
      })
    );
    // Forward: lead = 22 - 14 = 8 >= 4, trail = 10 - 7 = 3 <= 8.
    expect(result).toEqual({
      action: 'skip',
      visibleRange: { low: 10, high: 14 },
    });
  });

  it('skips at the end of the list where no more lead is reachable', () => {
    const result = resolveVisibleIndices(
      input({
        dataLength: 20,
        mountedRange: { low: 10, high: 19 },
        visibleStartIndex: 15,
        visibleEndIndex: 19,
        prevVisibleRange: { low: 14, high: 18 },
      })
    );
    // Lead = 0, but the reachable lead target is also 0 (band clamps at 19).
    expect(result).toEqual({
      action: 'skip',
      visibleRange: { low: 15, high: 19 },
    });
  });

  it('skips at the start of the list when scrolling backward into the clamp', () => {
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 0, high: 12 },
        visibleStartIndex: 2,
        visibleEndIndex: 6,
        prevVisibleRange: { low: 3, high: 7 },
      })
    );
    // Backward: lead = 2 - 0 = 2, target = 2 - max(0, 2 - 8) = 2, so 2 >= min(4, 2).
    expect(result).toEqual({
      action: 'skip',
      visibleRange: { low: 2, high: 6 },
    });
  });

  it('skips backward scrolling with enough lead below', () => {
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 2, high: 17 },
        prevVisibleRange: { low: 12, high: 16 },
      })
    );
    // Backward: lead = 10 - 2 = 8 >= 4, trail = 17 - 14 = 3 <= 8.
    expect(result).toEqual({
      action: 'skip',
      visibleRange: { low: 10, high: 14 },
    });
  });
});

describe('reband (hysteresis tripped)', () => {
  it('extends ahead when the forward lead drops below half the leading overscan', () => {
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 7, high: 17 },
        prevVisibleRange: { low: 9, high: 13 },
      })
    );
    // Lead = 17 - 14 = 3 < 4.
    expect(result).toEqual({
      action: 'reband',
      visibleRange: { low: 10, high: 14 },
      band: { low: 7, high: 22 },
    });
  });

  it('extends below when the backward lead drops below the threshold', () => {
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 8, high: 17 },
        prevVisibleRange: { low: 12, high: 16 },
      })
    );
    // Backward: lead = 10 - 8 = 2 < 4; band biased downward.
    expect(result).toEqual({
      action: 'reband',
      visibleRange: { low: 10, high: 14 },
      band: { low: 2, high: 17 },
    });
  });

  it('trims when the trail behind outgrows the allowance', () => {
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 0, high: 22 },
        prevVisibleRange: { low: 9, high: 13 },
      })
    );
    // Trail = 10 - 0 = 10 > 8; one reband both trims and re-biases.
    expect(result).toEqual({
      action: 'reband',
      visibleRange: { low: 10, high: 14 },
      band: { low: 7, high: 22 },
    });
  });

  it('re-biases immediately on a direction flip', () => {
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 7, high: 22 },
        prevVisibleRange: { low: 11, high: 15 },
      })
    );
    // Was scrolling forward (band led up to 22); now backward: lead below is
    // 10 - 7 = 3 < 4, so the band flips to lead downward.
    expect(result).toEqual({
      action: 'reband',
      visibleRange: { low: 10, high: 14 },
      band: { low: 2, high: 17 },
    });
  });

  it('treats the first event as scrolling forward', () => {
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 7, high: 17 },
        prevVisibleRange: NONE,
      })
    );
    expect(result).toMatchObject({ band: { low: 7, high: 22 } });
  });
});

describe('post-prepend recovery', () => {
  it('rebands once a union-shifted range trails too far behind', () => {
    // A prepend's anchor shift unions the mounted range (never shrinks) and the next
    // native emit reports the shifted visible indices; the grown trail trips the
    // hysteresis, so an ordinary reband re-centers the range on the band.
    const result = resolveVisibleIndices(
      input({
        mountedRange: { low: 0, high: 44 },
        visibleStartIndex: 30,
        visibleEndIndex: 34,
        prevVisibleRange: { low: 10, high: 14 },
      })
    );
    expect(result).toEqual({
      action: 'reband',
      visibleRange: { low: 30, high: 34 },
      band: { low: 27, high: 42 },
    });
  });
});

describe('mountedRange helpers', () => {
  it('seeds an empty list as unmounted', () => {
    expect(initialMountedRange(0, 20, false, -2)).toEqual({
      low: -1,
      high: -1,
    });
  });

  it('seeds a default list from the start', () => {
    expect(initialMountedRange(100, 20, false, -2)).toEqual({
      low: 0,
      high: 20,
    });
    expect(initialMountedRange(10, 20, false, -2)).toEqual({ low: 0, high: 9 });
  });

  it('seeds an inverted list anchored to the bottom', () => {
    expect(initialMountedRange(1000, 20, true, -2)).toEqual({
      low: 980,
      high: 999,
    });
    expect(initialMountedRange(10, 20, true, -2)).toEqual({ low: 0, high: 9 });
  });

  it('seeds around an explicit initial target, overriding inversion', () => {
    expect(initialMountedRange(100, 20, false, 50)).toEqual({
      low: 46,
      high: 70,
    });
    expect(initialMountedRange(100, 20, true, 50)).toEqual({
      low: 46,
      high: 70,
    });
    // Target clamped into the list; overscan clamped at the start.
    expect(initialMountedRange(10, 20, false, 50)).toEqual({ low: 5, high: 9 });
    expect(initialMountedRange(100, 20, false, 2)).toEqual({
      low: 0,
      high: 22,
    });
  });

  it('expands a range into indices and rejects invalid ranges', () => {
    expect(rangeToIndices({ low: 3, high: 6 })).toEqual([3, 4, 5, 6]);
    expect(rangeToIndices({ low: 5, high: 5 })).toEqual([5]);
    expect(rangeToIndices({ low: -1, high: -1 })).toEqual([]);
    expect(rangeToIndices({ low: 6, high: 3 })).toEqual([]);
  });
});
