import { useCallback, useMemo, useState } from 'react';
import { StyleSheet, Text, View } from 'react-native';
import {
  Reorder,
  ListFooter,
  createStyles,
  type ReorderTileItem,
} from 'shadowlist-utils/native';
import { haptics } from './haptics';
import { useHeaderMenu } from './HeaderActions';
import { DEBUG } from './launchSettings';
import { QueryStatus } from './QueryStatus';
import { useReorderWishlist, useWishlistQuery } from './queries/gallery';

const HINT = 'Touch and hold a sight, then drag it anywhere in the grid.';

export const ReorderGridScreen = () => {
  const styles = useStyles();
  const wishlist = useWishlistQuery();
  const { mutate: saveOrder } = useReorderWishlist();
  const [mixedSizes, setMixedSizes] = useState(false);
  const [lastMove, setLastMove] = useState('');

  useHeaderMenu([
    [
      {
        label: 'Even Sizes',
        symbol: 'square.grid.2x2',
        checked: !mixedSizes,
        onPress: () => setMixedSizes(false),
      },
      {
        label: 'Mixed Sizes',
        symbol: 'rectangle.grid.1x2',
        checked: mixedSizes,
        onPress: () => setMixedSizes(true),
      },
    ],
  ]);

  const handleReorder = useCallback(
    ({
      from,
      to,
      data: reordered,
    }: {
      from: number;
      to: number;
      data: ReorderTileItem[];
    }) => {
      haptics.drop();
      if (DEBUG) {
        const order = reordered.map((item) => item.label).join(',');
        console.log(`[SLJ] reorder from=${from} to=${to} order=${order}`);
        setLastMove(
          `Moved ${reordered[to]?.label} from ${from + 1} to ${to + 1}`
        );
      }
      saveOrder(reordered);
    },
    [saveOrder]
  );

  // With -SLDebug 1 the header also shows the last move, for scripted runs.
  const header = useMemo(
    () => (
      <View>
        <ListFooter text={HINT} />
        {DEBUG ? (
          <Text style={styles.statusText}>{lastMove || 'No moves yet'}</Text>
        ) : null}
      </View>
    ),
    [styles, lastMove]
  );

  if (wishlist.data === undefined) {
    return <QueryStatus error={wishlist.error} onRetry={wishlist.refetch} />;
  }

  return (
    <View style={styles.container}>
      <Reorder.Grid
        data={wishlist.data}
        style={styles.list}
        columns={3}
        tileAspectRatio={mixedSizes ? undefined : 1}
        onReorder={handleReorder}
        ListHeaderComponent={header}
      />
    </View>
  );
};

const useStyles = createStyles(({ colors, typography }) =>
  StyleSheet.create({
    container: {
      flex: 1,
      backgroundColor: colors.background,
    },
    list: {
      flex: 1,
      backgroundColor: colors.background,
    },
    statusText: {
      color: colors.secondaryLabel,
      ...typography.footnote,
      textAlign: 'center',
      paddingBottom: 12,
    },
  })
);
