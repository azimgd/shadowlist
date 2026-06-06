import { useState, useRef, useCallback, useMemo } from 'react';
import { View, StyleSheet, ActivityIndicator } from 'react-native';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist';
import { useHeaderActions } from './HeaderActions';
import {
  FeedElement,
  type FeedElement as FeedElementType,
} from './FeedElement';
import { HeaderListItem } from './HeaderListItem';
import { FooterListItem } from './FooterListItem';
import { generateFeedElement } from './constants';
import { colors } from './theme';

export const FeedScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<FeedElementType[]>(() =>
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
    () =>
      loadingMore ? (
        <View style={styles.loadingFooter}>
          <ActivityIndicator color={colors.secondaryLabel} />
        </View>
      ) : (
        <FooterListItem text="End of feed" />
      ),
    [loadingMore]
  );

  return (
    <View style={styles.container}>
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        autoHideHeader
        refreshing={refreshing}
        onRefresh={handleRefresh}
        refreshColor={colors.secondaryLabel}
        onEndReached={handleEndReached}
        renderElement={({ element, index }) => (
          <FeedElement element={element} index={index} />
        )}
        ListHeaderComponent={
          <HeaderListItem title="Feed" subtitle="Vertical scrolling list" />
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
  loadingFooter: {
    padding: 16,
    alignItems: 'center',
  },
});
