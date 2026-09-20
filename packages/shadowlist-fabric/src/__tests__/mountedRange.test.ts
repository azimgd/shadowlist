import { describe, expect, it } from '@jest/globals';
import {
  initialMountedRange,
  rangeToIndices,
  shouldReseedFromOffsetIndex,
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
    // A feed of full-screen cards mounts one row of runway, not ten.
    expect(initialMountedRange(1000, 2, false, 400, 1)).toEqual({
      low: 399,
      high: 402,
    });
  });

  /*
   * A centred or end-aligned jump fills the viewport with the rows BEFORE the target, so
   * seeding forwards only leaves that half blank until native reports the new window.
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
   * initialMountedRange is exported from the package root, so a caller written against the
   * four-argument form must not get a NaN range (which mounts nothing at all).
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
  // The case that rendered a chat blank: the target arrives one render after mount, and
  // without a reseed the viewport sits on rows React never rendered.
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
