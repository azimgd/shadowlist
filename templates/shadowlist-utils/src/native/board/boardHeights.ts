import type { BoardColumn } from './types';

/*
 * How a column's cards take their height. 'fill' stretches every column down the height the
 * board is given. 'content' makes every column's cards as tall as the tallest column's, within
 * minColumnHeight and maxColumnHeight, for a board inside a page that sizes itself to its cards.
 */
export type BoardColumnHeight = 'fill' | 'content';

/*
 * The height the cards of every column take in 'content' mode. A column that has not measured
 * its cards yet counts estimatedItemHeight for each. The result is never below min, and max wins
 * over min when they cross.
 */
export function boardColumnHeight<ItemT>(
  columns: ReadonlyArray<BoardColumn<ItemT>>,
  contentHeights: Readonly<Record<string, number>>,
  estimatedItemHeight: number,
  min: number,
  max: number
): number {
  let tallest = 0;
  for (const column of columns) {
    const height =
      contentHeights[column.key] ?? column.items.length * estimatedItemHeight;
    tallest = Math.max(tallest, height);
  }
  return Math.max(0, Math.min(max, Math.max(min, tallest)));
}
