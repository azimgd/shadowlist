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

  // Surface the live on-screen index range on the sticky footer.
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

  // Pagination indicator; the ref guards against onEndReached re-firing mid-load.
  const [loadingMore, setLoadingMore] = useState(false);
  const loadingRef = useRef(false);

  const handleEndReached = useCallback(() => {
    if (loadingRef.current) return;
    loadingRef.current = true;
    setLoadingMore(true);
    // Timeout stands in for a network fetch.
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

  // Pull-to-refresh: prepend a fresh batch, then clear the spinner.
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
  // Drop the 20th and 50th rows.
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
        // Open the list already scrolled to index 30.
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
