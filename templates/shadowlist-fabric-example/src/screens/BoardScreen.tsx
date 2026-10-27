import { memo } from 'react';
import { StyleSheet, View, type ColorValue } from 'react-native';
import {
  Board,
  ListFooter,
  createStyles,
  useDragBoardState,
  useTheme,
  type BoardColumn,
  type BoardMoveInfo,
  type ThemeColors,
} from 'shadowlist-utils/native';
import { haptics } from '../haptics';
import { DEBUG } from '../launchSettings';
import { HOLD_TO_DRAG } from './holdToDrag';

/*
 * A kanban board: three columns of cards side by side in a paged horizontal scroll, drawn by
 * ShadowListUtils' Board.List. Touch and hold a card to reorder it within its column or drag
 * it into another one, on iOS, Android and macOS. VoiceOver and TalkBack offer Move to for
 * each other column, named by its title. useDragBoardState holds the columns and applies every
 * move. The screen only gives the cards and a callback for the haptic and the log.
 */

type CardLabel = 'Design' | 'Bug' | 'Research' | 'Ops' | 'Copy';

interface Card {
  id: string;
  title: string;
  label: CardLabel;
  due: string;
}

const INITIAL_COLUMNS: ReadonlyArray<BoardColumn<Card>> = [
  {
    key: 'backlog',
    title: 'Backlog',
    items: [
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
  },
  {
    key: 'doing',
    title: 'In progress',
    items: [
      {
        id: 'c9',
        title: 'Trip timeline widget',
        label: 'Design',
        due: 'Oct 27',
      },
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
      {
        id: 'c12',
        title: 'Onboarding headlines',
        label: 'Copy',
        due: 'Oct 31',
      },
    ],
  },
  {
    key: 'done',
    title: 'Done',
    items: [
      {
        id: 'c13',
        title: 'Upgrade offer sheet',
        label: 'Design',
        due: 'Oct 20',
      },
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
  },
];

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

const BoardCard = memo(({ card }: { card: Card }) => {
  const { colors } = useTheme();
  return (
    <Board.Card
      title={card.title}
      label={card.label}
      detail={card.due}
      color={labelColor(colors, card.label)}
      accessibilityLabel={`${card.title}, ${card.label}, due ${card.due}`}
      testID={`board-card-${card.id}`}
    />
  );
});

const renderCard = ({ item }: { item: Card }) => <BoardCard card={item} />;

const handleMoveItem = ({
  item,
  sourceColumnKey,
  destinationColumnKey,
  destinationKey,
  destinationIndex,
}: BoardMoveInfo<Card>) => {
  haptics.drop();
  if (DEBUG) {
    console.log(
      `[SLJ] board move card=${item.id} from=${sourceColumnKey} to=${destinationColumnKey} index=${destinationIndex} before=${destinationKey}`
    );
  }
};

export const BoardScreen = () => {
  const styles = useStyles();
  const board = useDragBoardState(INITIAL_COLUMNS, {
    onMoveItem: handleMoveItem,
  });

  return (
    <View style={styles.container}>
      <ListFooter
        text={`${HOLD_TO_DRAG} a card, then drag it up, down or into another column.`}
      />
      <Board.List
        columns={board.columns}
        renderItem={renderCard}
        onMoveItem={board.moveItem}
        idPrefix="example-release-board"
        testID="board"
      />
    </View>
  );
};

const useStyles = createStyles(({ colors }) =>
  StyleSheet.create({
    container: {
      flex: 1,
      backgroundColor: colors.background,
    },
  })
);
