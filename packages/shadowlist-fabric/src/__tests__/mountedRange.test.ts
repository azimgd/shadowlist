import { describe, expect, it } from '@jest/globals';
import {
  MAX_FOLLOWED_APPEND,
  grownMountedRange,
  initialMountedRange,
  mountStepForWindow,
  rangeToIndices,
  reportedMountedRange,
  shouldReseedFromOffsetIndex,
  stepMountedRange,
  unionRangeIndices,
  visibleTargetRange,
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
   * A jump centered or aligned to the end shows the rows before the target. Mounting only
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
   * initialMountedRange is public. Old callers passing four arguments must not get a NaN
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

describe('mountStepForWindow', () => {
  it('keeps the minimum step for a window of a few tall rows', () => {
    expect(mountStepForWindow({ low: 10, high: 15 }, 2)).toBe(2);
  });

  it('grows the pad by a quarter of a window full of short rows', () => {
    // 36 short rows in the window, like a section list.
    expect(mountStepForWindow({ low: 875, high: 910 }, 2)).toBe(9);
  });

  it('reaches a ten row leading pad within a couple of steps on short rows', () => {
    const window = { low: 875, high: 910 };
    const step = mountStepForWindow(window, 2);
    let range = { low: 875, high: 914 };
    const target = { low: 865, high: 914 };
    range = stepMountedRange(range, target, window, step);
    range = stepMountedRange(range, target, window, step);
    expect(range).toEqual(target);
  });
});

describe('grownMountedRange', () => {
  it('keeps a range away from the data edges as is', () => {
    expect(grownMountedRange(30, 20, false, false, 100, 10, false)).toEqual({
      low: 20,
      high: 30,
    });
  });

  it('grows a range at an edge by the leading pad', () => {
    expect(grownMountedRange(5, 20, true, false, 100, 10, false)).toEqual({
      low: 0,
      high: 20,
    });
    expect(grownMountedRange(80, 95, false, true, 100, 10, false)).toEqual({
      low: 80,
      high: 99,
    });
  });

  it('follows every appended row at the tail of an inverted list', () => {
    // 30 rows arrived past the old end at 69, more than the pad.
    expect(grownMountedRange(60, 69, false, true, 100, 10, true)).toEqual({
      low: 60,
      high: 99,
    });
    // A burst past MAX_FOLLOWED_APPEND moves the range to the tail instead.
    const size = 70 + MAX_FOLLOWED_APPEND + 100;
    expect(grownMountedRange(60, 69, false, true, size, 10, true)).toEqual({
      low: size - 1 - 9 - MAX_FOLLOWED_APPEND,
      high: size - 1,
    });
  });
});

describe('visibleTargetRange', () => {
  it('pads both sides evenly on a first report', () => {
    expect(visibleTargetRange({ low: 50, high: 55 }, null, 200, 4, 12)).toEqual(
      { low: 46, high: 59 }
    );
  });

  it('pads ahead of the scroll direction', () => {
    expect(
      visibleTargetRange(
        { low: 50, high: 55 },
        { low: 45, high: 50 },
        200,
        4,
        12
      )
    ).toEqual({ low: 46, high: 67 });
    expect(
      visibleTargetRange(
        { low: 50, high: 55 },
        { low: 60, high: 65 },
        200,
        4,
        12
      )
    ).toEqual({ low: 38, high: 59 });
  });

  it('stays inside the data', () => {
    expect(visibleTargetRange({ low: 2, high: 8 }, null, 10, 4, 12)).toEqual({
      low: 0,
      high: 9,
    });
  });
});

describe('reportedMountedRange', () => {
  it('trims the initial range to the screen plus overscan on the first report', () => {
    expect(
      reportedMountedRange(
        { low: 0, high: 20 },
        { low: 0, high: 2 },
        null,
        true,
        100,
        4,
        10,
        2
      )
    ).toEqual({ range: { low: 0, high: 6 }, target: { low: 0, high: 6 } });
  });

  it('keeps a range that holds the screen on later reports', () => {
    expect(
      reportedMountedRange(
        { low: 0, high: 20 },
        { low: 0, high: 2 },
        { low: 0, high: 2 },
        false,
        100,
        4,
        10,
        2
      )
    ).toBeNull();
  });

  it('trims an inverted list at its tail and a range seeded around a target', () => {
    expect(
      reportedMountedRange(
        { low: 80, high: 99 },
        { low: 95, high: 99 },
        null,
        true,
        100,
        4,
        10,
        2
      )?.range
    ).toEqual({ low: 91, high: 99 });
    expect(
      reportedMountedRange(
        { low: 396, high: 420 },
        { low: 400, high: 403 },
        null,
        true,
        1000,
        4,
        10,
        2
      )?.range
    ).toEqual({ low: 396, high: 407 });
  });

  it('mounts the screen now and paces the pad when the window is past the range', () => {
    expect(
      reportedMountedRange(
        { low: 0, high: 20 },
        { low: 30, high: 33 },
        null,
        true,
        100,
        4,
        10,
        2
      )
    ).toEqual({ range: { low: 28, high: 35 }, target: { low: 26, high: 37 } });
    expect(
      reportedMountedRange(
        { low: 0, high: 20 },
        { low: 30, high: 33 },
        { low: 20, high: 23 },
        false,
        100,
        4,
        10,
        2
      )?.target
    ).toEqual({ low: 26, high: 43 });
  });
});
