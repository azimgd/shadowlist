import { memo, useCallback, useMemo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { ShadowList } from 'shadowlist';
import { createStyles } from 'shadowlist-utils/native';
import type { CarouselCard } from './fixtures/carousel';

export const CAROUSEL_SHELF_ID = 'carousel-shelf';

export interface CarouselShelfItem {
  id: typeof CAROUSEL_SHELF_ID;
  cards: CarouselCard[];
}

/*
 * A horizontal list with a header and cards of different widths.
 * Cards added before the visible ones must leave the cards on screen in place.
 */
export const CarouselShelf = memo(({ item }: { item: CarouselShelfItem }) => {
  const styles = useStyles();

  const renderCard = useCallback(
    ({ element }: { element: CarouselCard }) => (
      <View style={[styles.card, element.style]}>
        <Text style={styles.cardLabel}>{element.label}</Text>
      </View>
    ),
    [styles]
  );

  const header = useMemo(
    () => (
      <View style={styles.header}>
        <Text style={styles.headerTitle} accessibilityRole="header">
          Deals
        </Text>
        <Text style={styles.headerSubtitle}>Fares this week</Text>
      </View>
    ),
    [styles]
  );

  return (
    <View style={styles.row}>
      <ShadowList
        data={item.cards}
        horizontal
        style={styles.list}
        renderElement={renderCard}
        ListHeaderComponent={header}
      />
    </View>
  );
});

const useStyles = createStyles(({ colors, typography, spacing, radius }) =>
  StyleSheet.create({
    row: {
      height: 180,
      marginBottom: spacing.lg,
    },
    list: {
      flex: 1,
      backgroundColor: colors.background,
    },
    header: {
      width: 150,
      height: '100%',
      justifyContent: 'center',
      paddingHorizontal: spacing.lg,
    },
    headerTitle: {
      color: colors.label,
      ...typography.title3,
    },
    headerSubtitle: {
      color: colors.secondaryLabel,
      ...typography.subhead,
    },
    card: {
      flex: 1,
      marginHorizontal: 6,
      marginVertical: spacing.sm,
      borderRadius: radius.lg,
      padding: spacing.md,
      justifyContent: 'flex-end',
    },
    cardLabel: {
      color: colors.onAccent,
      ...typography.subhead,
    },
  })
);
