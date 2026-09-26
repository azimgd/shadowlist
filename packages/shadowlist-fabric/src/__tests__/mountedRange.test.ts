import { describe, expect, it } from '@jest/globals';
import {
  initialMountedRange,
  rangeToIndices,
  shouldReseedFromOffsetIndex,
  stepMountedRange,
  unionRangeIndices,
} from '../virtualizer/mountedRange';
import { SHADOWLIST_OVERSCAN } from '../virtualizer/helpers';

describe('initialMountedRange', () => {
  it('seeds around an explicit target rather than the head of the data', () => {
    const range = initialMountedRange(
      1000,
      20,
      false,
      400,
      SHADOWLIST_OVERSCAN
    );
    expect(range.low).toBe(400 - SHADOWLIST_OVERSCAN);
    expect(range.high).toBe(420);
    expect(rangeToIndices(range)).toContain(400);
  });

  it('treats every negative value as "no target", not only -2', () => {
    // -1 reads like an index sentinel and used to be mistaken for one.
    for (const off of [-1, -2, -99]) {
      expect(
        initialMountedRange(1000, 20, false, off, SHADOWLIST_OVERSCAN)
      ).toEqual({
        low: 0,
        high: 20,
      });
    }
  });

  it('anchors an inverted list to the end when it has no target', () => {
    expect(
      initialMountedRange(1000, 20, true, -2, SHADOWLIST_OVERSCAN)
    ).toEqual({
      low: 980,
      high: 999,
    });
  });

  it("seeds the window from the caller's overscanRows, not a fixed constant", () => {
    // A feed of full screen cards mounts one row ahead, not ten.
    expect(initialMountedRange(1000, 2, false, 400, 1)).toEqual({
      low: 399,
      high: 402,
    });
  });

  /*
   * A jump centered or aligned to the end shows the rows before the target, so mounting only
   * forward would leave that half blank until native reports.
   */
  it('seeds behind the target in proportion to viewPosition', () => {
    const centred = initialMountedRange(1000, 20, false, 400, 4, 0.5);
    expect(centred).toEqual({ low: 400 - 4 - 10, high: 410 });

    const atEnd = initialMountedRange(1000, 20, false, 400, 4, 1);
    expect(atEnd).toEqual({ low: 400 - 4 - 20, high: 400 });
  });

  it('keeps the start-aligned seed when viewPosition is omitted or zero', () => {
    const omitted = initialMountedRange(1000, 20, false, 400, 4);
    expect(initialMountedRange(1000, 20, false, 400, 4, 0)).toEqual(omitted);
    expect(omitted).toEqual({ low: 396, high: 420 });
  });

  /*
   * initialMountedRange is public, so old callers passing four arguments must not get a NaN
   * range, which mounts nothing.
   */
  it('falls back to the default overscan when the caller omits it', () => {
    expect(initialMountedRange(1000, 20, false, 400)).toEqual(
      initialMountedRange(1000, 20, false, 400, SHADOWLIST_OVERSCAN)
    );
    expect(rangeToIndices(initialMountedRange(1000, 20, false, 400))).toContain(
      400
    );
  });

  it('clamps a target past the end of the data', () => {
    const range = initialMountedRange(10, 20, false, 500, SHADOWLIST_OVERSCAN);
    expect(range.high).toBe(9);
    expect(range.low).toBeGreaterThanOrEqual(0);
  });
});

describe('shouldReseedFromOffsetIndex', () => {
  /*
   * This left a chat blank. The target arrives one render after mount, and without a
   * rebuild the screen sits on rows React never rendered.
   */
  it('reseeds when a target arrives after mount', () => {
    expect(shouldReseedFromOffsetIndex(-2, 340)).toBe(true);
    expect(shouldReseedFromOffsetIndex(-1, 340)).toBe(true);
  });

  it('reseeds when the target moves to another row', () => {
    expect(shouldReseedFromOffsetIndex(340, 12)).toBe(true);
  });

  it('does not reseed while the target is unchanged', () => {
    expect(shouldReseedFromOffsetIndex(340, 340)).toBe(false);
    expect(shouldReseedFromOffsetIndex(-2, -2)).toBe(false);
  });

  // Turning the target off must not drag a reader who has scrolled away back to it.
  it('does not reseed when the target is cleared', () => {
    expect(shouldReseedFromOffsetIndex(340, -2)).toBe(false);
  });
});

describe('unionRangeIndices', () => {
  it('keeps the rows on screen mounted next to a far scroll target', () => {
    expect(
      unionRangeIndices({ low: 2, high: 5 }, { low: 40, high: 42 })
    ).toEqual([2, 3, 4, 5, 40, 41, 42]);
    expect(
      unionRangeIndices({ low: 40, high: 42 }, { low: 2, high: 5 })
    ).toEqual([2, 3, 4, 5, 40, 41, 42]);
  });

  it('merges ranges that overlap or touch into one run', () => {
    expect(unionRangeIndices({ low: 2, high: 6 }, { low: 5, high: 8 })).toEqual(
      [2, 3, 4, 5, 6, 7, 8]
    );
    expect(unionRangeIndices({ low: 2, high: 4 }, { low: 5, high: 6 })).toEqual(
      [2, 3, 4, 5, 6]
    );
    expect(unionRangeIndices({ low: 0, high: 9 }, { low: 3, high: 4 })).toEqual(
      rangeToIndices({ low: 0, high: 9 })
    );
  });

  it('ignores an empty range', () => {
    expect(
      unionRangeIndices({ low: -1, high: -1 }, { low: 3, high: 4 })
    ).toEqual([3, 4]);
    expect(
      unionRangeIndices({ low: 3, high: 4 }, { low: -1, high: -1 })
    ).toEqual([3, 4]);
  });
});

describe('stepMountedRange', () => {
  it('grows the leading pad a few rows at a time', () => {
    expect(
      stepMountedRange(
        { low: 10, high: 20 },
        { low: 14, high: 32 },
        { low: 18, high: 21 },
        2
      )
    ).toEqual({ low: 14, high: 22 });
  });

  it('always mounts the rows on screen in the same step', () => {
    expect(
      stepMountedRange(
        { low: 10, high: 20 },
        { low: 16, high: 36 },
        { low: 22, high: 26 },
        2
      )
    ).toEqual({ low: 16, high: 26 });
  });

  it('shrinks right away and grows toward the start the same way', () => {
    expect(
      stepMountedRange(
        { low: 10, high: 20 },
        { low: 0, high: 16 },
        { low: 10, high: 12 },
        2
      )
    ).toEqual({ low: 8, high: 16 });
  });

  it('grows from the screen after a jump', () => {
    expect(
      stepMountedRange(
        { low: 0, high: 20 },
        { low: 496, high: 513 },
        { low: 500, high: 503 },
        2
      )
    ).toEqual({ low: 498, high: 505 });
  });

  it('lands on the target', () => {
    expect(
      stepMountedRange(
        { low: 14, high: 30 },
        { low: 14, high: 32 },
        { low: 18, high: 21 },
        2
      )
    ).toEqual({ low: 14, high: 32 });
  });
});
