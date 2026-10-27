/*
 * The board's horizontal layout. A column takes COLUMN_SHARE of the board on a phone, capped at
 * COLUMN_MAX_WIDTH for wide windows where several columns fit, with the next column's edge in
 * view as a hint to scroll sideways. COLUMN_GAP sits between columns and the snap interval is a
 * column and its gap, so the board always comes to rest on a column's leading edge.
 */
export const COLUMN_GAP = 12;
export const COLUMN_SHARE = 0.78;
export const COLUMN_MAX_WIDTH = 360;
export const SIDE_PADDING = 24;

export interface BoardColumnLayout {
  /** Every column's width in points. */
  columnWidth: number;
  /** The scroll content's padding before the first column and after the last. */
  sidePadding: number;
  /** The horizontal scroll's snapToInterval: a column and the gap after it. */
  snapInterval: number;
}

/*
 * The column width, side padding and snap interval of a board laid out boardWidth points wide.
 * A columnWidth that is a positive, finite number is used as it is for every column. Anything
 * else (undefined, zero, a negative width or NaN) takes the default share of the board. A
 * column narrower than the share keeps SIDE_PADDING on each side, and one as wide or wider is
 * centred, never with a negative padding.
 */
export function boardColumnLayout(
  boardWidth: number,
  columnWidth?: number
): BoardColumnLayout {
  const share = Math.round(boardWidth * COLUMN_SHARE);
  const width =
    columnWidth !== undefined && Number.isFinite(columnWidth) && columnWidth > 0
      ? columnWidth
      : Math.min(share, COLUMN_MAX_WIDTH);
  const sidePadding =
    width < share
      ? SIDE_PADDING
      : Math.max(0, Math.round((boardWidth - width) / 2));
  return {
    columnWidth: width,
    sidePadding,
    snapInterval: width + COLUMN_GAP,
  };
}
