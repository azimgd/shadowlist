import { useState, useRef, useCallback, useMemo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { type ShadowListCommands, type OnScroll } from 'shadowlist';
import { Activity, Spinner, colors, typography } from 'shadowlist-utils/native';
import {
  type ActivityData,
  buildActivity,
  useListController,
  START_REACHED_THRESHOLDS,
  END_REACHED_THRESHOLDS,
  nextInCycle,
  HEADER_HIDE_THRESHOLD,
} from 'shadowlist-utils';

export const ActivityScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const initialData = useMemo(
    () => Array.from({ length: 300 }, (_, index) => buildActivity(index)),
    []
  );

  /*
   * useListController owns the data + refreshing / loadingMore flags; the custom
   * scroll and viewability handlers below stay local and are passed straight to the list.
   */
  const list = useListController<ActivityData>({
    initialData,
    // Pull-to-refresh: prepend a fresh batch.
    onRefresh: () =>
      new Promise<void>((resolve) =>
        setTimeout(() => {
          list.prepend(
            Array.from({ length: 10 }, (_, index) => buildActivity(index))
          );
          resolve();
        }, 1200)
      ),
    // Pagination: append the next page. The hook guards against re-firing mid-load.
    onEndReached: () =>
      new Promise<void>((resolve) =>
        setTimeout(() => {
          list.append(
            Array.from({ length: 20 }, (_, index) =>
              buildActivity(list.data.length + index)
            )
          );
          resolve();
        }, 1000)
      ),
  });
  const { removeItems } = list;

  const [viewableLabel, setViewableLabel] = useState('—');
  const [startThreshold, setStartThreshold] = useState(1);
  const [endThreshold, setEndThreshold] = useState(1.5);
  const [headerSticky, setHeaderSticky] = useState(true);
  const headerStickyRef = useRef(true);

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

  // Hide the sticky header past the threshold, repin on the way back up.
  const handleScroll = useCallback((event: { nativeEvent: OnScroll }) => {
    const sticky = event.nativeEvent.contentOffsetY < HEADER_HIDE_THRESHOLD;
    if (sticky !== headerStickyRef.current) {
      headerStickyRef.current = sticky;
      setHeaderSticky(sticky);
    }
  }, []);

  // Drop the 20th and 50th rows.
  const handleRemoveItems = useCallback(
    () => removeItems((_, index) => index === 20 || index === 50),
    [removeItems]
  );

  const header = useMemo(
    () => (
      <Activity.Header
        title="Activity"
        subtitle="Imperative scroll, thresholds & list editing, opens at index 30"
        actions={[
          {
            label: 'Offset 2000',
            onPress: () => shadowlistRef.current?.scrollToOffset(2000),
          },
          {
            label: 'Scroll to end',
            onPress: () => shadowlistRef.current?.scrollToEnd(),
          },
          {
            label: `Start ×${startThreshold}`,
            onPress: () =>
              setStartThreshold((current) =>
                nextInCycle(START_REACHED_THRESHOLDS, current)
              ),
          },
          {
            label: `End ×${endThreshold}`,
            onPress: () =>
              setEndThreshold((current) =>
                nextInCycle(END_REACHED_THRESHOLDS, current)
              ),
          },
          { label: 'Remove 20 & 50', onPress: handleRemoveItems },
        ]}
      />
    ),
    [startThreshold, endThreshold, handleRemoveItems]
  );

  /*
   * Persistent full-width status footer: always shows the viewable range + total,
   * with the pagination spinner appended (not swapped in) so the info never
   * disappears while loading more.
   */
  const footer = useMemo(
    () => (
      <View style={styles.statusFooter}>
        <Text style={styles.statusText}>
          {`Viewable: ${viewableLabel} · Total: ${list.data.length}`}
        </Text>
        {list.loadingMore && <Spinner size={16} />}
      </View>
    ),
    [list.loadingMore, viewableLabel, list.data.length]
  );

  const renderElement = useCallback(
    ({ element }: { element: ActivityData }) => (
      <Activity.Row element={element} />
    ),
    []
  );

  return (
    <View style={styles.container}>
      <Activity.List
        data={list.data}
        ref={shadowlistRef}
        style={styles.list}
        renderElement={renderElement}
        containerOffsetIndex={30}
        stickyHeader={headerSticky}
        refreshing={list.refreshing}
        onRefresh={list.handleRefresh}
        refreshColor={colors.secondaryLabel}
        ListHeaderComponent={header}
        ListFooterComponent={footer}
        onScroll={handleScroll}
        onEndReached={list.handleEndReached}
        onStartReachedThreshold={startThreshold}
        onEndReachedThreshold={endThreshold}
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
    gap: 8,
    backgroundColor: colors.background,
    paddingHorizontal: 16,
    paddingVertical: 20,
  },
  statusText: {
    color: colors.secondaryLabel,
    ...typography.footnote,
  },
});
