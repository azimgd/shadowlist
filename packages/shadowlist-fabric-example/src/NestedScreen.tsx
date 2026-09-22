import { useMemo, useRef, useState } from 'react';
import { View } from 'react-native';
import { ShadowList, type ShadowListCommands } from 'shadowlist';
import { useInfiniteListProps } from 'shadowlist-utils';
import {
  Nested,
  ListHeader,
  ListFooter,
  Spinner,
  type NestedItem,
} from 'shadowlist-utils/native';
import {
  CAROUSEL_SHELF_ID,
  CarouselShelf,
  type CarouselShelfItem,
} from './CarouselShelf';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { request } from './api/network';
import { createCarouselCards } from './fixtures/carousel';
import { useShelvesQuery } from './queries/gallery';
import { useScreenStyles } from './screenStyles';

type ShelfRow = NestedItem | CarouselShelfItem;

const INITIAL_CARDS = 30;
const PAGE_CARDS = 8;

// The carousel keeps its scroll position when far off screen. Other shelves remount at the first card.
const PERSISTENT_KEYS = [CAROUSEL_SHELF_ID];

const HEADER = (
  <ListHeader
    title="Explore"
    subtitle="Destinations by mood; deals row stays pinned"
  />
);

const renderRow = ({ element }: { element: ShelfRow }) =>
  element.id === CAROUSEL_SHELF_ID ? (
    <CarouselShelf item={element as CarouselShelfItem} />
  ) : (
    <Nested.Row item={element as NestedItem} />
  );

export const NestedScreen = () => {
  const styles = useScreenStyles();
  const shadowlistRef = useRef<ShadowListCommands>(null);

  const shelves = useShelvesQuery();
  const list = useInfiniteListProps(shelves);

  const [cards, setCards] = useState(() =>
    createCarouselCards(INITIAL_CARDS, 'Deal')
  );
  const carousel = useMemo<CarouselShelfItem>(
    () => ({ id: CAROUSEL_SHELF_ID, cards }),
    [cards]
  );
  const data = useMemo<ShelfRow[]>(
    () => [carousel, ...list.data],
    [carousel, list.data]
  );

  useHeaderActions({
    onPrepend: () => {
      request(() => createCarouselCards(PAGE_CARDS, 'New')).then((created) =>
        setCards((previous) => [...created, ...previous])
      );
    },
    onAppend: () => {
      request(() => createCarouselCards(PAGE_CARDS, 'Fare')).then((created) =>
        setCards((previous) => [...previous, ...created])
      );
    },
    onScrollToRandom: () =>
      shadowlistRef.current?.scrollToIndex(
        Math.floor(Math.random() * data.length)
      ),
  });

  const { hasNextPage } = shelves;
  const footer = useMemo(
    () =>
      hasNextPage ? <Spinner /> : <ListFooter text="No more destinations" />,
    [hasNextPage]
  );

  if (shelves.data === undefined) {
    return <QueryStatus error={shelves.error} onRetry={shelves.refetch} />;
  }

  return (
    <View style={styles.container}>
      <ShadowList
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderElement={renderRow}
        persistentKeys={PERSISTENT_KEYS}
        onEndReached={list.onEndReached}
        ListHeaderComponent={HEADER}
        ListFooterComponent={footer}
      />
    </View>
  );
};
