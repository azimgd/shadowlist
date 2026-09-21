import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { Pressable, ScrollView, StyleSheet, Text, View } from 'react-native';
import {
  ShadowListNative,
  type ShadowListNativeCommands,
  type ShadowListNativeElementPressEvent,
} from 'shadowlist';
import {
  ListFooter,
  ListHeader,
  Spinner,
  createStyles,
  type NestedItem,
} from 'shadowlist-utils/native';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { fetchShelvesPage } from './api/gallery';
import { request } from './api/network';
import { createCarouselCards, type CarouselCard } from './fixtures/carousel';
import { generateNestedCard } from './fixtures/nested';
import { useScreenStyles } from './screenStyles';

const INITIAL_DEALS = 30;
const PAGE_DEALS = 8;
const CARD_WIDTH = 180;
const GRID_COLUMNS = 2;

interface CardRow {
  id: string;
  title: string;
  uri: string;
  liked: boolean;
}

interface ShelfRow {
  id: string;
  title: string;
  cards: CardRow[];
}

interface DealRow {
  id: string;
  label: string;
  width: number;
  color: string;
  picked: boolean;
}

function toShelf(shelf: NestedItem): ShelfRow {
  return {
    id: shelf.id,
    title: shelf.title,
    cards: shelf.cards.map((card) => ({
      id: card.id,
      title: card.title,
      uri: card.image.uri,
      liked: false,
    })),
  };
}

function toDeal(card: CarouselCard): DealRow {
  return {
    id: card.id,
    label: card.label,
    width: Number(card.style.width),
    color: String(card.style.backgroundColor),
    picked: false,
  };
}

let addedCards = 0;

/*
 * Explore on ShadowListNative. Three lists, all native rows:
 *
 * - the shelves: a vertical list whose template holds a horizontal ScrollView; the cards inside
 *   are a `repeat` over the shelf's `cards` array, cloned natively per shelf;
 * - the deals: a horizontal ShadowListNative in the list header (not recycled, so it keeps its
 *   position), with prepend/append from the header arrows;
 * - the grid: every loaded card in a two-column ShadowListNative.
 */
export const NestedNativeScreen = () => {
  const screenStyles = useScreenStyles();
  const styles = useStyles();
  const shelvesRef = useRef<ShadowListNativeCommands<ShelfRow>>(null);
  const dealsRef = useRef<ShadowListNativeCommands<DealRow>>(null);
  const gridRef = useRef<ShadowListNativeCommands<CardRow>>(null);

  const [initialShelves, setInitialShelves] = useState<ShelfRow[] | null>(null);
  const [error, setError] = useState<Error | null>(null);
  const [hasNextPage, setHasNextPage] = useState(true);
  const [mode, setMode] = useState<'shelves' | 'grid'>('shelves');
  const cursorRef = useRef<number | undefined>(undefined);
  const loadingRef = useRef(false);
  const [initialDeals] = useState(() =>
    createCarouselCards(INITIAL_DEALS, 'Deal').map(toDeal)
  );

  const loadFirstPage = useCallback(() => {
    setError(null);
    fetchShelvesPage(undefined).then((page) => {
      cursorRef.current = page.nextCursor;
      setHasNextPage(page.nextCursor !== undefined);
      setInitialShelves(page.items.map(toShelf));
    }, setError);
  }, []);

  useEffect(loadFirstPage, [loadFirstPage]);

  const loadMore = useCallback(() => {
    const cursor = cursorRef.current;
    if (loadingRef.current || cursor === undefined) return;
    loadingRef.current = true;
    fetchShelvesPage({ after: cursor })
      .then((page) => {
        cursorRef.current = page.nextCursor;
        setHasNextPage(page.nextCursor !== undefined);
        shelvesRef.current?.appendItems(page.items.map(toShelf));
      })
      .catch(() => {})
      .finally(() => {
        loadingRef.current = false;
      });
  }, []);

  useHeaderActions({
    onPrepend: () => {
      request(() => createCarouselCards(PAGE_DEALS, 'New')).then((created) =>
        dealsRef.current?.prependItems(created.map(toDeal))
      );
    },
    onAppend: () => {
      request(() => createCarouselCards(PAGE_DEALS, 'Fare')).then((created) =>
        dealsRef.current?.appendItems(created.map(toDeal))
      );
    },
    onScrollToRandom: () => {
      const list = mode === 'grid' ? gridRef.current : shelvesRef.current;
      list?.scrollToIndex(Math.floor(Math.random() * (list.getCount() || 1)));
    },
  });

  const handleShelfPress = useCallback(
    ({
      key,
      action,
      item,
      repeatIndex,
    }: ShadowListNativeElementPressEvent<ShelfRow>) => {
      const shelves = shelvesRef.current;
      if (!shelves || !item) return;
      if (action === 'top') {
        shelves.moveItem(key, 0);
      } else if (action === 'add') {
        addedCards += 1;
        const card = generateNestedCard(addedCards * 7);
        shelves.updateItem(key, {
          cards: [
            {
              id: card.id,
              title: `New · ${card.title}`,
              uri: card.image.uri,
              liked: false,
            },
            ...item.cards,
          ],
        });
      } else if (action === 'card' && repeatIndex !== undefined) {
        shelves.updateItem(key, {
          cards: item.cards.map((card, index) =>
            index === repeatIndex ? { ...card, liked: !card.liked } : card
          ),
        });
      }
    },
    []
  );

  const handleDealPress = useCallback(
    ({ key, item }: ShadowListNativeElementPressEvent<DealRow>) => {
      if (item) dealsRef.current?.updateItem(key, { picked: !item.picked });
    },
    []
  );

  const handleGridPress = useCallback(
    ({ key, action, item }: ShadowListNativeElementPressEvent<CardRow>) => {
      if (!item) return;
      if (action === 'like') {
        gridRef.current?.updateItem(key, { liked: !item.liked });
      } else if (action === 'remove') {
        gridRef.current?.removeItems([key]);
      }
    },
    []
  );

  // Grid mode shows every card of the shelves loaded so far, with their liked state.
  const [gridCards, setGridCards] = useState<CardRow[]>([]);
  const showGrid = useCallback(() => {
    const shelves = shelvesRef.current;
    if (!shelves) return;
    const cards = shelves
      .getKeys()
      .flatMap((key) => shelves.getItem(key)?.cards ?? []);
    setGridCards(cards);
    setMode('grid');
  }, []);

  const shuffleGrid = useCallback(() => {
    const grid = gridRef.current;
    const keys = grid?.getKeys() ?? [];
    if (!grid || keys.length < 2) return;
    // Moves a visible-ish card to the front: the rows between shift by one.
    const key =
      keys[1 + Math.floor(Math.random() * Math.min(9, keys.length - 1))]!;
    grid.moveItem(key, 0);
  }, []);

  const shelfTemplates = useMemo(
    () => ({
      shelf: (
        <View style={styles.shelf}>
          <View style={styles.shelfHeader}>
            <ShadowListNative.Text
              style={styles.shelfTitle}
              numberOfLines={1}
              bind={{ text: 'title' }}
            />
            <ShadowListNative.View action="add" style={styles.shelfAction}>
              <Text style={styles.shelfActionText}>＋ Card</Text>
            </ShadowListNative.View>
            <ShadowListNative.View action="top" style={styles.shelfAction}>
              <Text style={styles.shelfActionText}>↑ Top</Text>
            </ShadowListNative.View>
          </View>
          <ScrollView
            horizontal
            showsHorizontalScrollIndicator={false}
            style={styles.strip}
          >
            <ShadowListNative.View repeat="cards" style={styles.stripContent}>
              <ShadowListNative.View action="card" style={styles.card}>
                <View style={styles.imageFrame}>
                  <ShadowListNative.Image
                    style={styles.image}
                    resizeMode="cover"
                    bind={{ uri: 'uri' }}
                  />
                  <ShadowListNative.Text
                    style={styles.badge}
                    bind={{ visible: 'liked' }}
                  >
                    ♥
                  </ShadowListNative.Text>
                </View>
                <ShadowListNative.Text
                  style={styles.cardTitle}
                  numberOfLines={2}
                  bind={{ text: 'title' }}
                />
              </ShadowListNative.View>
            </ShadowListNative.View>
          </ScrollView>
        </View>
      ),
    }),
    [styles]
  );

  const dealTemplates = useMemo(
    () => ({
      deal: (
        <ShadowListNative.View
          action="pick"
          style={styles.deal}
          bind={{ width: 'width', backgroundColor: 'color' }}
        >
          <ShadowListNative.Text
            style={styles.dealLabel}
            bind={{ text: 'label' }}
          />
          <ShadowListNative.Text
            style={styles.dealPicked}
            bind={{ visible: 'picked' }}
          >
            Picked ✓
          </ShadowListNative.Text>
        </ShadowListNative.View>
      ),
    }),
    [styles]
  );

  const gridTemplates = useMemo(
    () => ({
      card: (
        <View style={styles.gridCell}>
          <ShadowListNative.View action="like" style={styles.gridFrame}>
            <ShadowListNative.Image
              style={styles.image}
              resizeMode="cover"
              bind={{ uri: 'uri' }}
            />
            <ShadowListNative.Text
              style={styles.badge}
              bind={{ visible: 'liked' }}
            >
              ♥
            </ShadowListNative.Text>
          </ShadowListNative.View>
          <View style={styles.gridMeta}>
            <ShadowListNative.Text
              style={styles.gridTitle}
              numberOfLines={1}
              bind={{ text: 'title' }}
            />
            <ShadowListNative.View action="remove" style={styles.gridRemove}>
              <Text style={styles.shelfActionText}>✕</Text>
            </ShadowListNative.View>
          </View>
        </View>
      ),
    }),
    [styles]
  );

  const dealsHeader = useMemo(
    () => (
      <View style={styles.dealsHeader}>
        <Text style={styles.dealsTitle} accessibilityRole="header">
          Deals
        </Text>
        <Text style={styles.dealsSubtitle}>
          Live fares; header arrows add more
        </Text>
      </View>
    ),
    [styles]
  );

  const listHeader = useMemo(
    () => (
      <View>
        <ListHeader
          title="Explore (Native)"
          subtitle="Shelves cloned natively; cards repeat per shelf"
        />
        <View style={styles.deals}>
          <ShadowListNative
            ref={dealsRef}
            data={initialDeals}
            templates={dealTemplates}
            horizontal
            style={screenStyles.list}
            onElementPress={handleDealPress}
            ListHeaderComponent={dealsHeader}
          />
        </View>
      </View>
    ),
    [
      styles,
      screenStyles,
      initialDeals,
      dealTemplates,
      handleDealPress,
      dealsHeader,
    ]
  );

  const footer = useMemo(
    () =>
      hasNextPage ? <Spinner /> : <ListFooter text="No more destinations" />,
    [hasNextPage]
  );

  if (initialShelves === null) {
    return <QueryStatus error={error} onRetry={loadFirstPage} />;
  }

  return (
    <View style={screenStyles.container}>
      <View style={mode === 'shelves' ? styles.fill : styles.hidden}>
        <ShadowListNative
          ref={shelvesRef}
          data={initialShelves}
          templates={shelfTemplates}
          style={screenStyles.list}
          onEndReached={loadMore}
          onElementPress={handleShelfPress}
          ListHeaderComponent={listHeader}
          ListFooterComponent={footer}
        />
      </View>
      {mode === 'grid' ? (
        <ShadowListNative
          ref={gridRef}
          data={gridCards}
          templates={gridTemplates}
          columns={GRID_COLUMNS}
          style={screenStyles.list}
          onElementPress={handleGridPress}
          ListHeaderComponent={
            <ListHeader
              title="All cards"
              subtitle="Two columns; tap to like, ✕ removes"
            />
          }
        />
      ) : null}
      <View style={styles.toolbar}>
        <ToolbarButton
          label={mode === 'grid' ? 'Shelves' : 'Grid'}
          onPress={mode === 'grid' ? () => setMode('shelves') : showGrid}
        />
        {mode === 'grid' ? (
          <ToolbarButton label="Shuffle" onPress={shuffleGrid} />
        ) : null}
      </View>
    </View>
  );
};

const ToolbarButton = ({
  label,
  onPress,
}: {
  label: string;
  onPress: () => void;
}) => {
  const styles = useStyles();
  return (
    <Pressable
      onPress={onPress}
      accessibilityRole="button"
      accessibilityLabel={label}
      style={({ pressed }) => [styles.button, pressed && styles.pressed]}
    >
      <Text style={styles.buttonText}>{label}</Text>
    </Pressable>
  );
};

const useStyles = createStyles(
  ({ colors, typography, spacing, radius, fontWeight }) =>
    StyleSheet.create({
      fill: {
        flex: 1,
      },
      // The shelves stay mounted behind the grid, so their positions survive a round trip.
      hidden: {
        display: 'none',
      },
      shelf: {
        height: 300,
        marginBottom: spacing.lg,
        backgroundColor: colors.background,
      },
      shelfHeader: {
        flexDirection: 'row',
        alignItems: 'center',
        paddingHorizontal: spacing.lg,
        marginBottom: spacing.md,
        gap: spacing.sm,
      },
      shelfTitle: {
        flex: 1,
        color: colors.label,
        ...typography.title3,
      },
      shelfAction: {
        paddingVertical: spacing.xxs,
        paddingHorizontal: spacing.sm,
        borderRadius: radius.md,
        backgroundColor: colors.elevated2,
      },
      shelfActionText: {
        ...typography.footnote,
        color: colors.accent,
        fontWeight: fontWeight.semibold,
      },
      strip: {
        flex: 1,
      },
      stripContent: {
        flexDirection: 'row',
        paddingRight: spacing.lg,
      },
      card: {
        width: CARD_WIDTH,
        marginLeft: spacing.lg,
      },
      imageFrame: {
        width: CARD_WIDTH,
        height: 220,
        borderRadius: radius.md,
        overflow: 'hidden',
        backgroundColor: colors.elevated2,
        marginBottom: spacing.sm,
      },
      image: {
        width: '100%',
        height: '100%',
      },
      badge: {
        position: 'absolute',
        top: spacing.sm,
        right: spacing.sm,
        color: colors.red,
        ...typography.title3,
      },
      cardTitle: {
        color: colors.label,
        ...typography.subhead,
      },
      deals: {
        height: 180,
        marginBottom: spacing.lg,
      },
      dealsHeader: {
        width: 150,
        height: '100%',
        justifyContent: 'center',
        paddingHorizontal: spacing.lg,
      },
      dealsTitle: {
        color: colors.label,
        ...typography.title3,
      },
      dealsSubtitle: {
        color: colors.secondaryLabel,
        ...typography.subhead,
      },
      deal: {
        flex: 1,
        marginHorizontal: 6,
        marginVertical: spacing.sm,
        borderRadius: radius.lg,
        padding: spacing.md,
        justifyContent: 'flex-end',
      },
      dealLabel: {
        color: colors.onAccent,
        ...typography.subhead,
      },
      dealPicked: {
        color: colors.onAccent,
        ...typography.caption,
      },
      gridCell: {
        padding: spacing.sm,
      },
      gridFrame: {
        aspectRatio: 1,
        borderRadius: radius.md,
        overflow: 'hidden',
        backgroundColor: colors.elevated2,
      },
      gridMeta: {
        flexDirection: 'row',
        alignItems: 'center',
        marginTop: spacing.xs,
      },
      gridTitle: {
        flex: 1,
        color: colors.label,
        ...typography.subhead,
      },
      gridRemove: {
        paddingHorizontal: spacing.sm,
        paddingVertical: spacing.xxs,
      },
      toolbar: {
        position: 'absolute',
        left: spacing.lg,
        right: spacing.lg,
        bottom: spacing.xl,
        flexDirection: 'row',
        gap: spacing.xs,
        padding: spacing.xs,
        borderRadius: radius.lg,
        backgroundColor: colors.elevated2,
      },
      button: {
        flex: 1,
        paddingVertical: spacing.sm,
        borderRadius: radius.md,
        alignItems: 'center',
      },
      pressed: {
        opacity: 0.4,
      },
      buttonText: {
        ...typography.footnote,
        color: colors.accent,
        fontWeight: fontWeight.semibold,
      },
    })
);
