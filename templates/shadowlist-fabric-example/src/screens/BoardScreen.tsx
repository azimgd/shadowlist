import { memo, useCallback, useMemo, useState } from 'react';
import {
  Platform,
  ScrollView,
  StyleSheet,
  Text,
  View,
  useWindowDimensions,
  type ColorValue,
  type LayoutChangeEvent,
} from 'react-native';
import {
  DraggableList,
  type MoveItemInfo,
  type MoveItemToListInfo,
} from 'shadowlist';
import {
  ListFooter,
  createStyles,
  useTheme,
  type ThemeColors,
} from 'shadowlist-utils/native';
import { haptics } from '../haptics';
import { DEBUG } from '../launchSettings';
import { HOLD_TO_DRAG } from './holdToDrag';

/*
 * A kanban board: three columns of cards side by side in a paged horizontal scroll. Each
 * column is a DraggableList on the same drag board. Touch and hold a card to reorder it within
 * its column or drag it into another one, on iOS and Android. VoiceOver and TalkBack offer
 * Move to for each other column, named by its title.
 */

/*
 * Board ids are global to the app: two screens that share one would join one board. This
 * screen's id names it alone.
 */
const BOARD_ID = 'example-release-board';

type ColumnId = 'backlog' | 'doing' | 'done';

type CardLabel = 'Design' | 'Bug' | 'Research' | 'Ops' | 'Copy';

interface Card {
  id: string;
  title: string;
  label: CardLabel;
  due: string;
}

interface Column {
  id: ColumnId;
  title: string;
}

type Board = Record<ColumnId, Card[]>;

const COLUMNS: ReadonlyArray<Column> = [
  { id: 'backlog', title: 'Backlog' },
  { id: 'doing', title: 'In progress' },
  { id: 'done', title: 'Done' },
];

const INITIAL_BOARD: Board = {
  backlog: [
    {
      id: 'c1',
      title: 'Seat map shows the wrong row letters',
      label: 'Bug',
      due: 'Oct 28',
    },
    {
      id: 'c2',
      title: 'Boarding pass dark mode',
      label: 'Design',
      due: 'Oct 30',
    },
    {
      id: 'c3',
      title: 'Interview five frequent flyers about rebooking',
      label: 'Research',
      due: 'Nov 2',
    },
    {
      id: 'c4',
      title: 'Rotate the push certificate',
      label: 'Ops',
      due: 'Nov 4',
    },
    {
      id: 'c5',
      title: 'Rewrite the delay notice so it says what to do next',
      label: 'Copy',
      due: 'Nov 5',
    },
    { id: 'c6', title: 'Gate change banner', label: 'Design', due: 'Nov 7' },
    {
      id: 'c7',
      title: 'Lounge pass expires a day early in Tokyo',
      label: 'Bug',
      due: 'Nov 8',
    },
    {
      id: 'c8',
      title: 'Baggage tracking survey',
      label: 'Research',
      due: 'Nov 11',
    },
  ],
  doing: [
    { id: 'c9', title: 'Trip timeline widget', label: 'Design', due: 'Oct 27' },
    {
      id: 'c10',
      title: 'Check-in crashes when the passport scan is cancelled',
      label: 'Bug',
      due: 'Oct 27',
    },
    {
      id: 'c11',
      title: 'Move receipts to the new store',
      label: 'Ops',
      due: 'Oct 29',
    },
    { id: 'c12', title: 'Onboarding headlines', label: 'Copy', due: 'Oct 31' },
  ],
  done: [
    { id: 'c13', title: 'Upgrade offer sheet', label: 'Design', due: 'Oct 20' },
    {
      id: 'c14',
      title: 'Wallet pass loses the gate after an update',
      label: 'Bug',
      due: 'Oct 21',
    },
    {
      id: 'c15',
      title: 'Status page on the new host',
      label: 'Ops',
      due: 'Oct 23',
    },
  ],
};

const COLUMN_GAP = 12;

const labelColor = (colors: ThemeColors, label: CardLabel): ColorValue => {
  switch (label) {
    case 'Design':
      return colors.blue;
    case 'Bug':
      return colors.red;
    case 'Research':
      return colors.green;
    case 'Ops':
      return colors.orange;
    case 'Copy':
      return colors.secondaryLabel;
  }
};

const isColumnId = (id: string): id is ColumnId =>
  COLUMNS.some((column) => column.id === id);

const cardKey = (card: Card) => card.id;

const BoardCard = memo(({ card }: { card: Card }) => {
  const styles = useStyles();
  const { colors } = useTheme();
  const accent = labelColor(colors, card.label);
  return (
    <View style={styles.cardCell}>
      <View
        style={styles.card}
        testID={`board-card-${card.id}`}
        accessible
        accessibilityLabel={`${card.title}, ${card.label}, due ${card.due}`}
      >
        <View style={[styles.cardAccent, { backgroundColor: accent }]} />
        <View style={styles.cardBody}>
          <Text style={styles.cardTitle}>{card.title}</Text>
          <View style={styles.cardMeta}>
            <View style={[styles.cardDot, { backgroundColor: accent }]} />
            <Text style={styles.cardLabel}>{card.label}</Text>
            <Text style={styles.cardDue}>{card.due}</Text>
          </View>
        </View>
      </View>
    </View>
  );
});

const renderCard = ({ item }: { item: Card }) => <BoardCard card={item} />;

interface BoardColumnProps {
  column: Column;
  cards: Card[];
  width: number;
  onReorder: (columnId: ColumnId, cards: Card[]) => void;
  onMoveToColumn: (
    from: ColumnId,
    card: Card,
    to: string,
    index: number,
    beforeKey: string
  ) => void;
}

const BoardColumn = memo(
  ({ column, cards, width, onReorder, onMoveToColumn }: BoardColumnProps) => {
    const styles = useStyles();

    const handleMoveItem = useCallback(
      ({ data }: MoveItemInfo<Card>) => {
        haptics.drop();
        onReorder(column.id, data);
      },
      [column.id, onReorder]
    );

    const handleMoveItemToList = useCallback(
      ({
        item,
        destinationListId,
        destinationIndex,
        destinationKey,
      }: MoveItemToListInfo<Card>) => {
        haptics.drop();
        onMoveToColumn(
          column.id,
          item,
          destinationListId,
          destinationIndex,
          destinationKey
        );
      },
      [column.id, onMoveToColumn]
    );

    return (
      <View
        style={[styles.column, { width }]}
        testID={`board-column-${column.id}`}
      >
        <View style={styles.columnHeader}>
          <Text style={styles.columnTitle}>{column.title}</Text>
          <Text style={styles.columnCount}>{cards.length}</Text>
        </View>
        <View style={styles.columnBody}>
          <DraggableList
            data={cards}
            keyExtractor={cardKey}
            renderItem={renderCard}
            style={styles.columnList}
            testID={`board-list-${column.id}`}
            dragBoardId={BOARD_ID}
            dragListId={column.id}
            dragListLabel={column.title}
            onMoveItem={handleMoveItem}
            onMoveItemToList={handleMoveItemToList}
          />
          {cards.length === 0 ? (
            <View style={styles.columnEmpty} pointerEvents="none">
              <Text style={styles.columnEmptyText}>Drop cards here</Text>
            </View>
          ) : null}
        </View>
      </View>
    );
  }
);

export const BoardScreen = () => {
  const styles = useStyles();
  const { width: windowWidth } = useWindowDimensions();
  const [board, setBoard] = useState<Board>(INITIAL_BOARD);

  /*
   * The board's own laid-out width. On macOS useWindowDimensions reports the screen, not
   * the window, and a resizable window can differ from both.
   */
  const [boardWidth, setBoardWidth] = useState(0);
  const handleBoardLayout = useCallback((event: LayoutChangeEvent) => {
    setBoardWidth(event.nativeEvent.layout.width);
  }, []);

  /*
   * A column takes most of the board on a phone, capped for wide windows where several
   * columns fit. The next column's edge stays in view as a hint to scroll sideways.
   */
  const width = boardWidth || windowWidth;
  const columnWidth = Math.min(Math.round(width * 0.78), 360);
  const sidePadding =
    columnWidth < Math.round(width * 0.78)
      ? 24
      : Math.round((width - columnWidth) / 2);

  const handleReorder = useCallback((columnId: ColumnId, cards: Card[]) => {
    if (DEBUG) {
      console.log(
        `[SLJ] board reorder column=${columnId} order=${cards.map(cardKey).join(',')}`
      );
    }
    setBoard((previous) => ({ ...previous, [columnId]: cards }));
  }, []);

  /*
   * Takes the card out of its column by id and inserts it in the other column before the card
   * keyed beforeKey, or at the end for an empty key. A key no longer in the column falls back
   * to the drop index. Both columns update in one state change. The card never shows in both
   * or neither.
   */
  const handleMoveToColumn = useCallback(
    (
      from: ColumnId,
      card: Card,
      to: string,
      index: number,
      beforeKey: string
    ) => {
      if (!isColumnId(to) || to === from) {
        return;
      }
      if (DEBUG) {
        console.log(
          `[SLJ] board move card=${card.id} from=${from} to=${to} index=${index} before=${beforeKey}`
        );
      }
      setBoard((previous) => {
        const source = previous[from].filter((item) => item.id !== card.id);
        const destination = previous[to].filter((item) => item.id !== card.id);
        const keyed =
          beforeKey === ''
            ? destination.length
            : destination.findIndex((item) => item.id === beforeKey);
        const at =
          keyed !== -1
            ? keyed
            : Math.max(0, Math.min(index, destination.length));
        destination.splice(at, 0, card);
        return { ...previous, [from]: source, [to]: destination };
      });
    },
    []
  );

  const contentStyle = useMemo(
    () => [styles.boardContent, { paddingHorizontal: sidePadding }],
    [styles, sidePadding]
  );

  return (
    <View style={styles.container} onLayout={handleBoardLayout}>
      <ListFooter
        text={
          Platform.OS === 'ios'
            ? `${HOLD_TO_DRAG} a card, then drag it up, down or into another column.`
            : `${HOLD_TO_DRAG} a card, then drag it up or down within its column.`
        }
      />
      <ScrollView
        horizontal
        style={styles.board}
        contentContainerStyle={contentStyle}
        contentInsetAdjustmentBehavior="never"
        showsHorizontalScrollIndicator={false}
        snapToInterval={columnWidth + COLUMN_GAP}
        snapToAlignment="start"
        decelerationRate="fast"
        testID="board-scroll"
      >
        {COLUMNS.map((column) => (
          <BoardColumn
            key={column.id}
            column={column}
            cards={board[column.id]}
            width={columnWidth}
            onReorder={handleReorder}
            onMoveToColumn={handleMoveToColumn}
          />
        ))}
      </ScrollView>
    </View>
  );
};

const useStyles = createStyles(
  ({ colors, spacing, radius, typography, fontWeight }) =>
    StyleSheet.create({
      container: {
        flex: 1,
        backgroundColor: colors.background,
      },
      board: {
        flex: 1,
      },
      boardContent: {
        gap: COLUMN_GAP,
        paddingBottom: spacing.xl,
      },
      column: {
        backgroundColor: colors.elevated,
        borderRadius: radius.lg,
        overflow: 'hidden',
      },
      columnHeader: {
        flexDirection: 'row',
        alignItems: 'center',
        justifyContent: 'space-between',
        paddingHorizontal: spacing.lg,
        paddingTop: spacing.md,
        paddingBottom: spacing.sm,
      },
      columnTitle: {
        ...typography.headline,
        color: colors.label,
      },
      columnCount: {
        ...typography.subhead,
        fontWeight: fontWeight.semibold,
        color: colors.secondaryLabel,
        fontVariant: ['tabular-nums'],
      },
      columnBody: {
        flex: 1,
      },
      columnList: {
        flex: 1,
        backgroundColor: colors.elevated,
      },
      columnEmpty: {
        ...StyleSheet.absoluteFill,
        alignItems: 'center',
        justifyContent: 'center',
      },
      columnEmptyText: {
        ...typography.footnote,
        color: colors.tertiaryLabel,
      },
      /*
       * The cell stays transparent. The column paints the background, and a picked up card
       * then lifts as just the rounded card instead of a slab of column color.
       */
      cardCell: {
        paddingHorizontal: spacing.md,
        paddingBottom: spacing.sm,
      },
      card: {
        flexDirection: 'row',
        backgroundColor: colors.background,
        borderRadius: radius.md,
        borderWidth: StyleSheet.hairlineWidth,
        borderColor: colors.separator,
        overflow: 'hidden',
      },
      cardAccent: {
        width: 4,
      },
      cardBody: {
        flex: 1,
        paddingHorizontal: spacing.md,
        paddingVertical: spacing.md,
        gap: spacing.xs,
      },
      cardTitle: {
        ...typography.subhead,
        color: colors.label,
      },
      cardMeta: {
        flexDirection: 'row',
        alignItems: 'center',
        gap: spacing.xs,
      },
      cardDot: {
        width: 8,
        height: 8,
        borderRadius: 4,
      },
      cardLabel: {
        ...typography.caption,
        fontWeight: fontWeight.semibold,
        color: colors.secondaryLabel,
        flex: 1,
      },
      cardDue: {
        ...typography.caption,
        color: colors.tertiaryLabel,
        fontVariant: ['tabular-nums'],
      },
    })
);
