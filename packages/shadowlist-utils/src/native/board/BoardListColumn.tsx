import { memo, useCallback, type ReactElement } from 'react';
import {
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import {
  DraggableList,
  type MoveItemInfo,
  type MoveItemToListInfo,
  type ShadowListProps,
} from 'shadowlist';
import { createStyles } from '../theme';
import { boardListId } from './boardIds';
import { moveBetweenColumns, moveWithinColumn } from './boardMoves';
import type { BoardLabels } from './labels';
import type { BoardColumn, BoardMoveInfo } from './types';

interface BoardListColumnProps<ItemT> {
  column: BoardColumn<ItemT>;
  boardId: string;
  width: number;
  bodyHeight: number | undefined;
  scrollEnabled: boolean;
  onContentHeight: ((columnKey: string, height: number) => void) | undefined;
  style: StyleProp<ViewStyle>;
  renderHeader:
    ((column: BoardColumn<ItemT>) => ReactElement | null) | undefined;
  renderFooter:
    ((column: BoardColumn<ItemT>) => ReactElement | null) | undefined;
  renderItem: ShadowListProps<ItemT>['renderItem'];
  keyExtractor: (item: ItemT) => string;
  onMoveItem: ((info: BoardMoveInfo<ItemT>) => void) | undefined;
  labels: BoardLabels;
  testID: string | undefined;
}

/*
 * One column of a board: its header with the title and the count, and a DraggableList on the
 * board's drag board under it. Each list's dragListId comes from the board id and the column
 * key, and its title names it in the Move to action. An empty column says where cards go, or
 * that it has none while cards cannot move. renderHeader takes the default header's place, and
 * renderFooter draws under the cards, outside the list, so it never lifts and never takes a
 * place among the cards. A bodyHeight fixes the cards' height for a board sized to its content,
 * where the list reports its own content height through onContentHeight.
 */
function BoardListColumnInner<ItemT>({
  column,
  boardId,
  width,
  bodyHeight,
  scrollEnabled,
  onContentHeight,
  style,
  renderHeader,
  renderFooter,
  renderItem,
  keyExtractor,
  onMoveItem,
  labels,
  testID,
}: BoardListColumnProps<ItemT>) {
  const styles = useStyles();

  const handleMoveItem = useCallback(
    (info: MoveItemInfo<ItemT>) => {
      const move = moveWithinColumn(column.key, info, keyExtractor);
      if (move !== undefined) {
        onMoveItem?.(move);
      }
    },
    [column.key, keyExtractor, onMoveItem]
  );

  const handleMoveItemToList = useCallback(
    (info: MoveItemToListInfo<ItemT>) => {
      const move = moveBetweenColumns(boardId, column.key, info);
      if (move !== undefined) {
        onMoveItem?.(move);
      }
    },
    [boardId, column.key, onMoveItem]
  );

  const handleContentSizeChange = useCallback(
    (_width: number, height: number) => onContentHeight?.(column.key, height),
    [column.key, onContentHeight]
  );

  const count = column.items.length;
  const movable = onMoveItem !== undefined;
  const sizesToContent = bodyHeight !== undefined;
  return (
    <View
      style={[styles.column, { width }, style]}
      testID={
        testID !== undefined ? `${testID}-column-${column.key}` : undefined
      }
    >
      {renderHeader !== undefined ? (
        renderHeader(column)
      ) : (
        <View
          style={styles.header}
          accessible
          accessibilityRole="header"
          accessibilityLabel={labels.columnSummary(column.title, count)}
        >
          <Text style={styles.title}>{column.title}</Text>
          <Text style={styles.count}>{count}</Text>
        </View>
      )}
      <View style={sizesToContent ? { height: bodyHeight } : styles.body}>
        <DraggableList
          data={column.items}
          keyExtractor={keyExtractor}
          renderItem={renderItem}
          style={styles.list}
          testID={
            testID !== undefined ? `${testID}-list-${column.key}` : undefined
          }
          reorderEnabled={movable}
          dragBoardId={boardId}
          dragListId={boardListId(boardId, column.key)}
          dragListLabel={column.title}
          onMoveItem={handleMoveItem}
          onMoveItemToList={handleMoveItemToList}
          onContentSizeChange={
            onContentHeight !== undefined ? handleContentSizeChange : undefined
          }
          scrollEnabled={scrollEnabled}
          nestedScrollEnabled={sizesToContent ? true : undefined}
        />
        {count === 0 ? (
          <View style={styles.empty} pointerEvents="none">
            <Text style={styles.emptyText}>
              {movable ? labels.emptyColumn : labels.emptyColumnReadOnly}
            </Text>
          </View>
        ) : null}
      </View>
      {renderFooter?.(column) ?? null}
    </View>
  );
}

export const BoardListColumn = memo(BoardListColumnInner) as <ItemT>(
  props: BoardListColumnProps<ItemT>
) => ReactElement;

const useStyles = createStyles(
  ({ colors, spacing, radius, typography, fontWeight }) =>
    StyleSheet.create({
      column: {
        backgroundColor: colors.elevated,
        borderRadius: radius.lg,
        overflow: 'hidden',
      },
      header: {
        flexDirection: 'row',
        alignItems: 'center',
        justifyContent: 'space-between',
        paddingHorizontal: spacing.lg,
        paddingTop: spacing.md,
        paddingBottom: spacing.sm,
      },
      title: {
        ...typography.headline,
        color: colors.label,
        flexShrink: 1,
      },
      count: {
        ...typography.subhead,
        fontWeight: fontWeight.semibold,
        color: colors.secondaryLabel,
        fontVariant: ['tabular-nums'],
      },
      body: {
        flex: 1,
      },
      list: {
        flex: 1,
      },
      empty: {
        ...StyleSheet.absoluteFill,
        alignItems: 'center',
        justifyContent: 'center',
      },
      emptyText: {
        ...typography.footnote,
        color: colors.tertiaryLabel,
      },
    })
);
