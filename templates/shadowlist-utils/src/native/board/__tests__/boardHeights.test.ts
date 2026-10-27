import { boardColumnHeight } from '../boardHeights';
import type { BoardColumn } from '../types';

const column = (key: string, count: number): BoardColumn<number> => ({
  key,
  title: key,
  items: Array.from({ length: count }, (_, index) => index),
});

const COLUMNS = [column('todo', 3), column('doing', 1), column('done', 0)];

describe('boardColumnHeight', () => {
  it('estimates the columns that have not measured their cards', () => {
    expect(boardColumnHeight(COLUMNS, {}, 60, 100, 1000)).toBe(180);
  });

  it('takes the tallest measured or estimated column', () => {
    expect(
      boardColumnHeight(COLUMNS, { todo: 90, doing: 240 }, 60, 0, 1000)
    ).toBe(240);
    expect(boardColumnHeight(COLUMNS, { todo: 90 }, 60, 0, 1000)).toBe(90);
  });

  it('keeps the height between min and max, max winning when they cross', () => {
    expect(boardColumnHeight(COLUMNS, { todo: 10 }, 60, 120, 1000)).toBe(120);
    expect(boardColumnHeight(COLUMNS, { todo: 5000 }, 60, 120, 600)).toBe(600);
    expect(boardColumnHeight(COLUMNS, {}, 60, 300, 200)).toBe(200);
  });

  it('ignores the measured height of a column no longer on the board', () => {
    expect(boardColumnHeight(COLUMNS, { gone: 900 }, 60, 0, 1000)).toBe(180);
  });

  it('is min for a board without cards', () => {
    expect(boardColumnHeight([column('todo', 0)], {}, 60, 120, 600)).toBe(120);
  });
});
