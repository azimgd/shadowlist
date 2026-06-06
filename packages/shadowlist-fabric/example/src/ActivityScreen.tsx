import { useState, useRef, useCallback, useMemo } from 'react';
import { View, StyleSheet, ActivityIndicator } from 'react-native';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist';
import { ActivityElement } from './ActivityElement';
import { ActivityHeader } from './ActivityHeader';
import { FooterListItem } from './FooterListItem';
import { ListItemSeparator } from './ListItemSeparator';
import { type ActivityData, buildActivity } from 'shadowlist-utils';

export const ActivityScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<ActivityData[]>(() =>
    Array.from({ length: 200 }, (_, index) => buildActivity(index))
  );
  const [viewableLabel, setViewableLabel] = useState('—');

  // viewability: surface the live on-screen index range on the sticky footer.
  const handleViewableItemsChanged = useCallback(
    ({ viewableItems }: { viewableItems: { index: number }[] }) => {
      if (viewableItems.length === 0) {
        setViewableLabel('—');
        return;
      }
      const first = viewableItems[0]!.index;
      const last = viewableItems[viewableItems.length - 1]!.index;
      setViewableLabel(`${first}–${last}`);
    },
    []
  );

  // Loading-more (pagination) indicator. There's no dedicated prop — render an
  // ActivityIndicator in ListFooterComponent while a loading flag is set (the
  // companion to onEndReached). The ref guards against the edge re-firing mid-load.
  const [loadingMore, setLoadingMore] = useState(false);
  const loadingRef = useRef(false);

  const handleEndReached = useCallback(() => {
    if (loadingRef.current) return;
    loadingRef.current = true;
    setLoadingMore(true);
    // The timeout stands in for a network fetch.
    setTimeout(() => {
      setData((prev) => [
        ...prev,
        ...Array.from({ length: 20 }, (_, index) =>
          buildActivity(prev.length + index)
        ),
      ]);
      setLoadingMore(false);
      loadingRef.current = false;
    }, 1000);
  }, []);

  // Pull-to-refresh: prepend a fresh batch, then clear the spinner. The native
  // refresh control is the loading indicator here (the footer ActivityIndicator above
  // is the companion for loading *more* at the end). The timeout stands in for a fetch.
  const [refreshing, setRefreshing] = useState(false);

  const handleRefresh = useCallback(() => {
    setRefreshing(true);
    setTimeout(() => {
      setData((prev) => [
        ...Array.from({ length: 10 }, (_, index) => buildActivity(index)),
        ...prev,
      ]);
      setRefreshing(false);
    }, 1200);
  }, []);

  const handleScrollToOffset = useCallback(
    () => shadowlistRef.current?.scrollToOffset(2000),
    []
  );
  const handleScrollToEnd = useCallback(
    () => shadowlistRef.current?.scrollToEnd(),
    []
  );
  // editing: drop the 20th and 50th rows to show keyed reconciliation.
  const handleRemoveItems = useCallback(
    () =>
      setData((prev) =>
        prev.filter((_, index) => index !== 20 && index !== 50)
      ),
    []
  );

  const header = useMemo(
    () => (
      <ActivityHeader
        onScrollToOffset={handleScrollToOffset}
        onScrollToEnd={handleScrollToEnd}
        onRemoveItems={handleRemoveItems}
      />
    ),
    [handleScrollToOffset, handleScrollToEnd, handleRemoveItems]
  );

  const footer = useMemo(
    () =>
      loadingMore ? (
        <View style={styles.loadingFooter}>
          <ActivityIndicator color="#FF9500" />
        </View>
      ) : (
        <FooterListItem text={`Viewable indices: ${viewableLabel}`} />
      ),
    [loadingMore, viewableLabel]
  );

  return (
    <View style={styles.container}>
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        keyExtractor={(item) => item.id}
        renderElement={({ element }) => <ActivityElement element={element} />}
        // Initial scroll position: open the list already scrolled to index 30. The
        // core anchors that row to the viewport top and its rows mount on the first
        // render (no scroll-from-top, no blank flash). Keep it constant for a pure
        // initial position.
        containerOffsetIndex={30}
        stickyHeader
        stickyFooter
        refreshing={refreshing}
        onRefresh={handleRefresh}
        refreshColor="#FF9500"
        ListHeaderComponent={header}
        ListFooterComponent={footer}
        ItemSeparatorComponent={<ListItemSeparator />}
        onEndReached={handleEndReached}
        viewabilityConfig={{ itemVisiblePercentThreshold: 60 }}
        onViewableItemsChanged={handleViewableItemsChanged}
      />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000000',
  },
  list: {
    flex: 1,
    backgroundColor: '#000000',
  },
  loadingFooter: {
    padding: 16,
    alignItems: 'center',
  },
});
