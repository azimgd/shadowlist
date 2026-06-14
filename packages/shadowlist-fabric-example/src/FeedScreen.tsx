import { useState, useRef, useCallback, useMemo } from 'react';
import { View, StyleSheet } from 'react-native';
import { type ShadowlistCommands } from 'shadowlist';
import {
  Feed,
  ListHeader,
  ListFooter,
  Spinner,
  colors,
} from 'shadowlist-utils/native';
import { generateFeedElement, type FeedItem } from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

export const FeedScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<FeedItem[]>(() =>
    Array.from({ length: 1000 }, (_, index) => generateFeedElement(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateFeedElement(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateFeedElement(currentLength + index)
    );
    setData((prev) => [...prev, ...newElements]);
  };

  const handleScrollToRandom = () => {
    shadowlistRef.current?.scrollToIndex(
      Math.floor(Math.random() * data.length)
    );
  };

  useHeaderActions({
    onPrepend: handlePrepend,
    onAppend: handleAppend,
    onScrollToRandom: handleScrollToRandom,
  });

  const [refreshing, setRefreshing] = useState(false);

  // Pull-to-refresh: prepend a fresh batch, then stop refreshing.
  const handleRefresh = useCallback(() => {
    setRefreshing(true);
    setTimeout(() => {
      setData((prev) => [
        ...Array.from({ length: 10 }, (_, index) => generateFeedElement(index)),
        ...prev,
      ]);
      setRefreshing(false);
    }, 1200);
  }, []);

  // Infinite scroll: append the next page on end reached.
  // loadingRef guards against re-firing while a load is in flight.
  const [loadingMore, setLoadingMore] = useState(false);
  const loadingRef = useRef(false);

  const handleEndReached = useCallback(() => {
    if (loadingRef.current) return;
    loadingRef.current = true;
    setLoadingMore(true);
    setTimeout(() => {
      setData((prev) => [
        ...prev,
        ...Array.from({ length: 20 }, (_, index) =>
          generateFeedElement(prev.length + index)
        ),
      ]);
      setLoadingMore(false);
      loadingRef.current = false;
    }, 1000);
  }, []);

  const footer = useMemo(
    () => (loadingMore ? <Spinner /> : <ListFooter text="End of feed" />),
    [loadingMore]
  );

  const renderElement = useCallback(
    ({ element, index }: { element: FeedItem; index: number }) => (
      <Feed.Element element={element} index={index} />
    ),
    []
  );

  return (
    <View style={styles.container}>
      <Feed.List
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        refreshing={refreshing}
        onRefresh={handleRefresh}
        refreshColor={colors.secondaryLabel}
        onEndReached={handleEndReached}
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
