import { useState, useRef, useCallback, useMemo } from 'react';
import { View, Text, StyleSheet, ActivityIndicator } from 'react-native';
import { type ShadowlistCommands } from 'shadowlist';
import { Activity, colors, typography } from 'shadowlist-utils/native';
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
      <Activity.Header
        title="Activity"
        subtitle="Imperative scroll & list editing, opens at index 30"
        actions={[
          { label: 'Offset 2000', onPress: handleScrollToOffset },
          { label: 'Scroll to end', onPress: handleScrollToEnd },
          { label: 'Remove 20 & 50', onPress: handleRemoveItems },
        ]}
      />
    ),
    [handleScrollToOffset, handleScrollToEnd, handleRemoveItems]
  );

  // Persistent full-width status footer: kept on screen by stickyFooter and always showing
  // the viewable range + total count. The pagination spinner is appended, not swapped in, so
  // the info never disappears on load or while loading more.
  const footer = useMemo(
    () => (
      <View style={styles.statusFooter}>
        <Text style={styles.statusText}>
          {`Viewable: ${viewableLabel} · Total: ${data.length}`}
        </Text>
        {loadingMore && (
          <ActivityIndicator
            size="small"
            color={colors.secondaryLabel}
            style={styles.statusSpinner}
          />
        )}
      </View>
    ),
    [loadingMore, viewableLabel, data.length]
  );

  return (
    <View style={styles.container}>
      <Activity.List
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        keyExtractor={(item) => item.id}
        renderElement={({ element }) => <Activity.Row element={element} />}
        containerOffsetIndex={30}
        refreshing={refreshing}
        onRefresh={handleRefresh}
        refreshColor={colors.secondaryLabel}
        ListHeaderComponent={header}
        ListFooterComponent={footer}
        onEndReached={handleEndReached}
        onViewableItemsChanged={handleViewableItemsChanged}
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
  statusFooter: {
    width: '100%',
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    backgroundColor: colors.background,
    paddingHorizontal: 16,
    paddingVertical: 20,
  },
  statusText: {
    color: colors.secondaryLabel,
    ...typography.footnote,
  },
  statusSpinner: {
    marginLeft: 8,
  },
});
