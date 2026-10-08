import { describe, expect, it, jest } from '@jest/globals';
import {
  ViewabilityTracker,
  viewableRulesFor,
} from '../virtualizer/viewability';
import { PrefetchTracker, prefetchWindow } from '../virtualizer/prefetch';
import {
  deselectKey,
  retainKeys,
  selectKey,
  selectedIndices,
} from '../virtualizer/selection';
import {
  contentPadding,
  rowPaddingStyles,
} from '../virtualizer/contentPadding';
import { SeparatorStore } from '../virtualizer/separators';
import type { ViewToken } from '../types';

const keys = ['a', 'b', 'c', 'd', 'e', 'f'];
const keyToIndex = new Map(keys.map((key, index) => [key, index]));
const build = (low: number, high: number): ViewToken<string>[] =>
  keys.slice(low, high + 1).map((key, offset) => ({
    item: key,
    key,
    index: low + offset,
    isViewable: true,
  }));

describe('viewableRulesFor', () => {
  it('starts with any overlap and adds one rule per config', () => {
    expect(viewableRulesFor([])).toEqual([0, 0]);
    expect(
      viewableRulesFor([
        { itemVisiblePercentThreshold: 50 },
        { viewAreaCoveragePercentThreshold: 30 },
      ])
    ).toEqual([0, 0, 0.5, 0, 0.3, 1]);
  });
});

describe('ViewabilityTracker', () => {
  it('reports changes by key', () => {
    const callback = jest.fn();
    const tracker = new ViewabilityTracker<string>({}, callback);
    tracker.update({ low: 0, high: 1 }, build);
    tracker.update({ low: 0, high: 1 }, build);
    tracker.update({ low: 1, high: 2 }, build);
    expect(callback).toHaveBeenCalledTimes(2);
    const last = callback.mock.calls[1]![0] as {
      changed: ViewToken<string>[];
    };
    expect(last.changed.map((token) => [token.key, token.isViewable])).toEqual([
      ['c', true],
      ['a', false],
    ]);
  });

  it('waits for an interaction', () => {
    const callback = jest.fn();
    const tracker = new ViewabilityTracker<string>(
      { waitForInteraction: true },
      callback
    );
    tracker.update({ low: 0, high: 1 }, build);
    expect(callback).not.toHaveBeenCalled();
    tracker.recordInteraction(build);
    expect(callback).toHaveBeenCalledTimes(1);
  });

  it('reports only after the minimum view time', () => {
    const callbacks: (() => void)[] = [];
    const timers = {
      setTimeout: (callback: () => void) => callbacks.push(callback),
      clearTimeout: () => {
        callbacks.length = 0;
      },
    };
    const callback = jest.fn();
    const tracker = new ViewabilityTracker<string>(
      { minimumViewTime: 100 },
      callback,
      timers
    );
    tracker.update({ low: 0, high: 1 }, build);
    tracker.update({ low: 2, high: 3 }, build);
    expect(callback).not.toHaveBeenCalled();
    callbacks.forEach((fire) => fire());
    expect(callback).toHaveBeenCalledTimes(1);
    const info = callback.mock.calls[0]![0] as {
      viewableItems: ViewToken<string>[];
    };
    expect(info.viewableItems.map((token) => token.key)).toEqual(['c', 'd']);
  });
});

describe('PrefetchTracker', () => {
  it('prefetches unmounted rows once and cancels the ones that leave', () => {
    const tracker = new PrefetchTracker();
    const first = tracker.update(
      keys,
      keyToIndex,
      { low: 0, high: 1 },
      {
        low: 0,
        high: 3,
      }
    );
    expect(first).toEqual({ prefetch: [2, 3], cancel: [] });
    expect(
      tracker.update(keys, keyToIndex, { low: 0, high: 1 }, { low: 0, high: 3 })
    ).toEqual({ prefetch: [], cancel: [] });
    // Row 2 mounted, row 3 left the window unseen.
    expect(
      tracker.update(keys, keyToIndex, { low: 1, high: 2 }, { low: 1, high: 2 })
    ).toEqual({ prefetch: [], cancel: [3] });
  });

  it('builds the window around the mounted rows', () => {
    expect(prefetchWindow({ low: 3, high: 5 }, 2, 7)).toEqual({
      low: 1,
      high: 6,
    });
    expect(prefetchWindow(null, 2, 7)).toBeNull();
  });
});

describe('selection', () => {
  it('replaces a single selection and adds to a multiple one', () => {
    expect(selectKey(['a'], 'b', false)).toEqual(['b']);
    expect(selectKey(['a'], 'b', true)).toEqual(['a', 'b']);
    const same = ['a'];
    expect(selectKey(same, 'a', false)).toBe(same);
    expect(deselectKey(['a', 'b'], 'a')).toEqual(['b']);
  });

  it('drops removed rows and reports indices low to high', () => {
    const selected = ['c', 'a'];
    expect(retainKeys(selected, keyToIndex)).toBe(selected);
    expect(retainKeys(['a', 'gone'], keyToIndex)).toEqual(['a']);
    expect(selectedIndices(selected, keyToIndex)).toEqual([0, 2]);
  });
});

describe('content padding', () => {
  it('splits padding along and across the scroll axis', () => {
    expect(
      contentPadding(
        { paddingVertical: 8, paddingHorizontal: 4 },
        { top: 2 },
        false
      )
    ).toEqual({ leading: 10, trailing: 8, crossStart: 4, crossEnd: 4 });
    expect(contentPadding({ paddingLeft: 6 }, undefined, true).leading).toBe(6);
  });

  it('keeps every column the same width', () => {
    const styles = rowPaddingStyles(3, 0, 0, { columnGap: 12 }, false)!;
    const widths = styles.map(
      (style) =>
        100 - ((style.paddingLeft as number) + (style.paddingRight as number))
    );
    expect(new Set(widths.map((width) => width.toFixed(6))).size).toBe(1);
    expect(styles[0]!.paddingLeft).toBe(0);
    expect(rowPaddingStyles(1, 0, 0, undefined, false)).toBeNull();
  });
});

describe('SeparatorStore', () => {
  it('notifies only the changed row', () => {
    const store = new SeparatorStore();
    const listener = jest.fn();
    const other = jest.fn();
    store.subscribe('a', listener);
    store.subscribe('b', other);
    store.setHighlighted('a', true);
    store.setHighlighted('a', true);
    expect(listener).toHaveBeenCalledTimes(1);
    expect(other).not.toHaveBeenCalled();
    expect(store.get('a').highlighted).toBe(true);
    store.setHighlighted('a', false);
    expect(store.get('a')).toBe(store.get('missing'));
  });
});
