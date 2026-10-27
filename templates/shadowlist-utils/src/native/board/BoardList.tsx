import { useCallback, useMemo, useState } from 'react';
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
  columnWidth?: number;
  labels?: Partial<BoardLabels>;
  style?: StyleProp<ViewStyle>;
  testID?: string;
}

const COLUMN_GAP = 12;
const COLUMN_SHARE = 0.78;
const COLUMN_MAX_WIDTH = 360;
const SIDE_PADDING = 24;

/*
 * A board of columns side by side in a horizontal scroll that snaps to each column. Every
 * column is a DraggableList on one drag board. Touch and hold a card, or drag it with the
 * mouse on macOS, to reorder it in its column or carry it into another one. onMoveItem gets
 * both kinds of move in one shape. Pass it useDragBoardState's moveItem, or apply each move
 * with moveBoardItem. Without onMoveItem the cards stay put. The board names its drag board
 * and lists itself and never shares them with another board. idPrefix only makes the ids
 * readable.
 */
export function BoardList<ItemT>({
  columns,
  renderItem,
  keyExtractor = defaultBoardKeyExtractor,
  onMoveItem,
  idPrefix,
  columnWidth,
  labels,
  style,
  testID,
}: BoardListProps<ItemT>) {
  const styles = useStyles();
  const boardLabels = useLabels(defaultBoardLabels, labels);
  const boardId = useBoardId(idPrefix);
  const { width: windowWidth } = useWindowDimensions();

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
   * columns fit. The next column's edge stays in view as a hint to scroll sideways.
   */
  const width = boardWidth || windowWidth;
  const share = Math.round(width * COLUMN_SHARE);
  const resolvedColumnWidth = columnWidth ?? Math.min(share, COLUMN_MAX_WIDTH);
  const sidePadding =
    resolvedColumnWidth < share
      ? SIDE_PADDING
      : Math.max(0, Math.round((width - resolvedColumnWidth) / 2));

  const contentStyle = useMemo(
    () => [styles.content, { paddingHorizontal: sidePadding }],
    [styles, sidePadding]
  );

  return (
    <ScrollView
      horizontal
      style={[styles.board, style]}
      contentContainerStyle={contentStyle}
      contentInsetAdjustmentBehavior="never"
      showsHorizontalScrollIndicator={false}
      snapToInterval={resolvedColumnWidth + COLUMN_GAP}
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
