import {
  boardColumnLayout,
  COLUMN_GAP,
  COLUMN_MAX_WIDTH,
  SIDE_PADDING,
} from '../boardWidths';

describe('boardColumnLayout', () => {
  it('gives a column 78% of a phone-wide board, centred', () => {
    expect(boardColumnLayout(400)).toEqual({
      columnWidth: 312,
      sidePadding: 44,
      snapInterval: 312 + COLUMN_GAP,
    });
  });

  it('caps the default width on a wide board and keeps the side padding', () => {
    expect(boardColumnLayout(1200)).toEqual({
      columnWidth: COLUMN_MAX_WIDTH,
      sidePadding: SIDE_PADDING,
      snapInterval: COLUMN_MAX_WIDTH + COLUMN_GAP,
    });
  });

  it('uses a given width as it is for the column and the snap interval', () => {
    expect(boardColumnLayout(400, 280)).toEqual({
      columnWidth: 280,
      sidePadding: SIDE_PADDING,
      snapInterval: 280 + COLUMN_GAP,
    });
    expect(boardColumnLayout(400, 300.5).columnWidth).toBe(300.5);
    expect(boardColumnLayout(1200, 500)).toEqual({
      columnWidth: 500,
      sidePadding: SIDE_PADDING,
      snapInterval: 500 + COLUMN_GAP,
    });
  });

  it('centres a given width as wide as the share or wider, never below zero', () => {
    expect(boardColumnLayout(400, 340).sidePadding).toBe(30);
    expect(boardColumnLayout(400, 600).sidePadding).toBe(0);
  });

  it('uses a given side padding as it is, whatever the width', () => {
    expect(boardColumnLayout(400, undefined, 16)).toEqual({
      columnWidth: 312,
      sidePadding: 16,
      snapInterval: 312 + COLUMN_GAP,
    });
    expect(boardColumnLayout(400, 280, 16).sidePadding).toBe(16);
    expect(boardColumnLayout(400, 600, 20).sidePadding).toBe(20);
    expect(boardColumnLayout(1200, undefined, 0).sidePadding).toBe(0);
    expect(boardColumnLayout(400, 280, 12.5).sidePadding).toBe(12.5);
  });

  it('ignores a side padding that is not a finite number of zero or more', () => {
    for (const padding of [-1, Number.NaN, Number.POSITIVE_INFINITY]) {
      expect(boardColumnLayout(400, undefined, padding)).toEqual(
        boardColumnLayout(400)
      );
      expect(boardColumnLayout(400, 280, padding)).toEqual(
        boardColumnLayout(400, 280)
      );
    }
  });

  it('ignores a width that is not a positive number', () => {
    const fallback = boardColumnLayout(400);
    for (const width of [0, -120, Number.NaN, Number.POSITIVE_INFINITY]) {
      expect(boardColumnLayout(400, width)).toEqual(fallback);
    }
  });
});
