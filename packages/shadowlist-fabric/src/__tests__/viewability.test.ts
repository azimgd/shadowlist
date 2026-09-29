import { describe, expect, it } from '@jest/globals';
import {
  activeStickyIndexFor,
  viewableWindow,
} from '../virtualizer/viewability';

describe('viewableWindow', () => {
  it('sorts an inverted report and drops an empty one', () => {
    expect(viewableWindow(12, 4)).toEqual({ low: 4, high: 12 });
    expect(viewableWindow(4, 12)).toEqual({ low: 4, high: 12 });
    expect(viewableWindow(-1, -1)).toBeNull();
    expect(viewableWindow(3, -1)).toBeNull();
  });
});

describe('activeStickyIndexFor', () => {
  it('picks the last section header at or above the window', () => {
    const headers = [0, 10, 25];
    expect(activeStickyIndexFor(headers, 0)).toBe(0);
    expect(activeStickyIndexFor(headers, 9)).toBe(0);
    expect(activeStickyIndexFor(headers, 10)).toBe(10);
    expect(activeStickyIndexFor(headers, 400)).toBe(25);
  });

  it('is -1 before the first header or without headers', () => {
    expect(activeStickyIndexFor([3, 8], 1)).toBe(-1);
    expect(activeStickyIndexFor([], 5)).toBe(-1);
    expect(activeStickyIndexFor(undefined, 5)).toBe(-1);
  });
});
