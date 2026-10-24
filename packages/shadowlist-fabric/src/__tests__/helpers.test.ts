import { describe, expect, it, jest } from '@jest/globals';
import {
  viewableRulesFor,
  viewableTimingsFor,
} from '../virtualizer/viewability';
import { mergeSortedIndices } from '../virtualizer/helpers';
import {
  deselectKey,
  retainKeys,
  selectKey,
  selectedIndices,
} from '../virtualizer/selection';
import {
  contentPadding,
  crossPadding,
  footerPaddingStyles,
  headerPaddingStyles,
  rowPaddingStyles,
} from '../virtualizer/contentPadding';
import { SeparatorStore } from '../virtualizer/separators';

const keys = ['a', 'b', 'c', 'd', 'e', 'f'];
const keyToIndex = new Map(keys.map((key, index) => [key, index]));
const indexOfKey = (key: string) => keyToIndex.get(key) ?? -1;

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

describe('viewableTimingsFor', () => {
  it('sends minimumViewTime and waitForInteraction per config', () => {
    expect(viewableTimingsFor([])).toEqual([]);
    expect(
      viewableTimingsFor([
        { minimumViewTime: 250 },
        { waitForInteraction: true },
        { minimumViewTime: -5 },
      ])
    ).toEqual([250, 0, 0, 1, 0, 0]);
  });
});

describe('mergeSortedIndices', () => {
  it('adds the missing rows in order and keeps the array when none is missing', () => {
    const mounted = [4, 5, 6, 7];
    expect(mergeSortedIndices(mounted, [])).toBe(mounted);
    expect(mergeSortedIndices(mounted, [5, 7])).toBe(mounted);
    expect(mergeSortedIndices(mounted, [0, 6, 40])).toEqual([
      0, 4, 5, 6, 7, 40,
    ]);
    expect(mergeSortedIndices([], [2])).toEqual([2]);
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
    expect(retainKeys(selected, indexOfKey)).toBe(selected);
    expect(retainKeys(['a', 'gone'], indexOfKey)).toEqual(['a']);
    expect(selectedIndices(selected, indexOfKey)).toEqual([0, 2]);
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

  it('pads the header and footer templates', () => {
    expect(crossPadding(0, 0, false)).toBeNull();
    const cross = crossPadding(4, 6, false);
    expect(cross).toEqual({ paddingLeft: 4, paddingRight: 6 });
    expect(crossPadding(4, 6, true)).toEqual({
      paddingTop: 4,
      paddingBottom: 6,
    });
    expect(headerPaddingStyles(null, 0, false)).toBeNull();
    expect(footerPaddingStyles(null, 0, true)).toBeNull();
    expect(headerPaddingStyles(cross, 8, false)).toEqual([
      cross,
      { paddingTop: 8 },
    ]);
    expect(footerPaddingStyles(null, 8, true)).toEqual([
      {},
      { paddingRight: 8 },
    ]);
    expect(footerPaddingStyles(cross, 0, false)).toEqual([cross, {}]);
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
