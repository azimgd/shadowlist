import { useCallback, useMemo, useState, type ReactElement } from 'react';
import {
  ScrollView,
  StyleSheet,
  useWindowDimensions,
  type LayoutChangeEvent,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import type { ShadowListProps } from 'shadowlist';
import { useLabels } from '../labels';
import { createStyles } from '../theme';
import { boardColumnHeight, type BoardColumnHeight } from './boardHeights';
import { boardColumnLayout, COLUMN_GAP } from './boardWidths';
import { useBoardId } from './boardIds';
import { BoardListColumn } from './BoardListColumn';
import { defaultBoardLabels, type BoardLabels } from './labels';
import { defaultBoardKeyExtractor } from './moveBoardItem';
import type { BoardColumn, BoardMoveInfo } from './types';

export interface BoardListProps<ItemT> {
  columns: ReadonlyArray<BoardColumn<ItemT>>;
  renderItem: ShadowListProps<ItemT>['renderItem'];
  keyExtractor?: (item: ItemT) => string;
  onMoveItem?: (info: BoardMoveInfo<ItemT>) => void;
  idPrefix?: string;
  /*
   * Every column's width in points, used as it is for the columns and the snap interval. Not
   * set, or not a positive number, a column takes 78% of the board, at most 360.
   */
  columnWidth?: number;
  /*
   * The inset in points before the first column and after the last, used as it is. Not set,
   * or not a finite number of zero or more, it is 24 for a column narrower than 78% of the
   * board, and a wider column is centred.
   */
  sidePadding?: number;
  columnHeight?: BoardColumnHeight;
  minColumnHeight?: number;
  maxColumnHeight?: number;
  estimatedItemHeight?: number;
  columnStyle?: StyleProp<ViewStyle>;
  renderColumnHeader?: (column: BoardColumn<ItemT>) => ReactElement | null;
  renderColumnFooter?: (column: BoardColumn<ItemT>) => ReactElement | null;
  labels?: Partial<BoardLabels>;
  style?: StyleProp<ViewStyle>;
  testID?: string;
}

/*
 * The 'content' height bounds when the board does not set them: room for a card or two to land
 * in an empty column, and a share of the window that leaves the page room around the board.
 * A column that has not measured its cards counts ESTIMATED_ITEM_HEIGHT for each.
 */
const MIN_COLUMN_HEIGHT = 120;
const MAX_COLUMN_HEIGHT_SHARE = 0.6;
const ESTIMATED_ITEM_HEIGHT = 64;

/*
 * A board of columns side by side in a horizontal scroll that snaps to each column. Every
 * column is a DraggableList on one drag board. Touch and hold a card, or drag it with the
 * mouse on macOS, to reorder it in its column or carry it into another one. onMoveItem gets
 * both kinds of move in one shape. Pass it useDragBoardState's moveItem, or apply each move
 * with moveBoardItem. Without onMoveItem the cards stay put. The board names its drag board
 * and lists itself and never shares them with another board. idPrefix only makes the ids
 * readable.
 *
 * By default the columns fill the height the board is given. With columnHeight 'content' the
 * board sizes itself instead: every column's cards are as tall as the tallest column's,
 * between minColumnHeight and maxColumnHeight, so each column is a drop target down the whole
 * board, and a column taller than that scrolls inside while one that fits leaves the swipe to
 * the page. renderColumnFooter draws a column's own trailing view under its cards, such as an
 * add row. It is never lifted or dragged, and a card let go over it lands in that column where
 * its gap last showed.
 */
export function BoardList<ItemT>({
  columns,
  renderItem,
  keyExtractor = defaultBoardKeyExtractor,
  onMoveItem,
  idPrefix,
  columnWidth,
  sidePadding: sidePaddingProp,
  columnHeight = 'fill',
  minColumnHeight = MIN_COLUMN_HEIGHT,
  maxColumnHeight,
  estimatedItemHeight = ESTIMATED_ITEM_HEIGHT,
  columnStyle,
  renderColumnHeader,
  renderColumnFooter,
  labels,
  style,
  testID,
}: BoardListProps<ItemT>) {
  const styles = useStyles();
  const boardLabels = useLabels(defaultBoardLabels, labels);
  const boardId = useBoardId(idPrefix);
  const { width: windowWidth, height: windowHeight } = useWindowDimensions();

  /*
   * The board's own laid out width. On macOS useWindowDimensions reports the screen, not the
   * window, and a resizable window can differ from both.
   */
  const [boardWidth, setBoardWidth] = useState(0);
  const handleLayout = useCallback((event: LayoutChangeEvent) => {
    setBoardWidth(event.nativeEvent.layout.width);
  }, []);

  /*
   * A column takes most of the board on a phone, capped for wide windows where several
   * columns fit, unless columnWidth sets it. The next column's edge stays in view as a hint
   * to scroll sideways, and the snap interval follows the width.
   */
  const {
    columnWidth: resolvedColumnWidth,
    sidePadding,
    snapInterval,
  } = boardColumnLayout(
    boardWidth || windowWidth,
    columnWidth,
    sidePaddingProp
  );

  /*
   * Each column's measured card height, kept only in 'content' mode, where it sets the height
   * of every column's cards.
   */
  const [contentHeights, setContentHeights] = useState<
    Readonly<Record<string, number>>
  >({});
  const sizesToContent = columnHeight === 'content';
  const handleContentHeight = useCallback(
    (columnKey: string, height: number) =>
      setContentHeights((current) =>
        current[columnKey] === height
          ? current
          : { ...current, [columnKey]: height }
      ),
    []
  );
  const bodyHeight = sizesToContent
    ? boardColumnHeight(
        columns,
        contentHeights,
        estimatedItemHeight,
        minColumnHeight,
        maxColumnHeight ?? Math.round(windowHeight * MAX_COLUMN_HEIGHT_SHARE)
      )
    : undefined;

  const contentStyle = useMemo(
    () => [styles.content, { paddingHorizontal: sidePadding }],
    [styles, sidePadding]
  );

  return (
    <ScrollView
      horizontal
      style={[sizesToContent ? undefined : styles.board, style]}
      contentContainerStyle={contentStyle}
      contentInsetAdjustmentBehavior="never"
      showsHorizontalScrollIndicator={false}
      snapToInterval={snapInterval}
      snapToAlignment="start"
      decelerationRate="fast"
      onLayout={handleLayout}
      testID={testID}
    >
      {columns.map((column) => (
        <BoardListColumn
          key={column.key}
          column={column}
          boardId={boardId}
          width={resolvedColumnWidth}
          bodyHeight={bodyHeight}
          scrollEnabled={
            bodyHeight === undefined ||
            (contentHeights[column.key] ?? 0) > bodyHeight
          }
          onContentHeight={sizesToContent ? handleContentHeight : undefined}
          style={columnStyle}
          renderHeader={renderColumnHeader}
          renderFooter={renderColumnFooter}
          renderItem={renderItem}
          keyExtractor={keyExtractor}
          onMoveItem={onMoveItem}
          labels={boardLabels}
          testID={testID}
        />
      ))}
    </ScrollView>
  );
}

const useStyles = createStyles(({ spacing }) =>
  StyleSheet.create({
    board: {
      flex: 1,
    },
    content: {
      gap: COLUMN_GAP,
      paddingBottom: spacing.xl,
    },
  })
);
