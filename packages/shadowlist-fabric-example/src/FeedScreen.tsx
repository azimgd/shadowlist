import { useRef, useCallback, useMemo } from 'react';
import { View, StyleSheet } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import {
  Feed,
  ListHeader,
  ListFooter,
  Spinner,
  colors,
} from 'shadowlist-utils/native';
import {
  generateFeedElement,
  useListController,
  type FeedItem,
} from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

const REFRESH_BATCH = 10;
const PAGE_SIZE = 20;

const batch = (count: number, offset: number): FeedItem[] =>
  Array.from({ length: count }, (_, index) =>
    generateFeedElement(offset + index)
  );

export const FeedScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);

  const initialData = useMemo(() => batch(1000, 0), []);

  /*
   * useListController owns the data plus the refreshing / loadingMore flags; each
   * `handle*` flips its flag while the async work runs and won't double-fire.
   */
  const list = useListController<FeedItem>({
    initialData,
    // Pull-to-refresh: prepend a fresh batch after a short delay.
    onRefresh: () =>
      new Promise<void>((resolve) =>
        setTimeout(() => {
          list.prepend(batch(REFRESH_BATCH, 0));
          resolve();
        }, 1200)
      ),
    // Infinite scroll: append the next page when the end is reached.
    onEndReached: () =>
      new Promise<void>((resolve) =>
        setTimeout(() => {
          list.append(batch(PAGE_SIZE, list.data.length));
          resolve();
        }, 1000)
      ),
  });

  const handlePrepend = () => list.prepend(batch(10, list.data.length));
  const handleAppend = () => list.append(batch(10, list.data.length));
  const handleScrollToRandom = () =>
    shadowlistRef.current?.scrollToIndex(
      Math.floor(Math.random() * list.data.length)
    );

  useHeaderActions({
    onPrepend: handlePrepend,
    onAppend: handleAppend,
    onScrollToRandom: handleScrollToRandom,
  });

  const footer = useMemo(
    () => (list.loadingMore ? <Spinner /> : <ListFooter text="End of feed" />),
    [list.loadingMore]
  );

  const renderElement = useCallback(
    ({ element }: { element: FeedItem }) => <Feed.Element element={element} />,
    []
  );

  return (
    <View style={styles.container}>
      <Feed.List
        data={list.data}
        ref={shadowlistRef}
        style={styles.list}
        refreshing={list.refreshing}
        onRefresh={list.handleRefresh}
        refreshColor={colors.secondaryLabel}
        onEndReached={list.handleEndReached}
        renderElement={renderElement}
        ListHeaderComponent={
          <ListHeader title="Feed" subtitle="Vertical scrolling list" />
        }
        ListFooterComponent={footer}
      />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: colors.background,
  },
  list: {
    flex: 1,
    backgroundColor: colors.background,
  },
});
