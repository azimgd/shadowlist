import { useState, useRef, useCallback, useMemo, type CSSProperties } from 'react';
import { type ShadowlistCommands, type OnScroll } from 'shadowlist-wasm';
import { Activity, ListFooter, colors } from 'shadowlist-utils/web';
import {
  type ActivityData,
  buildActivity,
  START_REACHED_THRESHOLDS,
  END_REACHED_THRESHOLDS,
  nextInCycle,
  HEADER_HIDE_THRESHOLD,
} from 'shadowlist-utils';

export const ActivityScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<ActivityData[]>(() =>
    Array.from({ length: 500 }, (_, index) => buildActivity(index))
  );
  const [viewableLabel, setViewableLabel] = useState('—');
  const [startThreshold, setStartThreshold] = useState(1);
  const [endThreshold, setEndThreshold] = useState(1.5);
  const [headerSticky, setHeaderSticky] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
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

  // Hide the sticky header past the threshold, re-pin on the way back up.
  const handleScroll = useCallback((event: { nativeEvent: OnScroll }) => {
    const sticky = event.nativeEvent.contentOffsetY < HEADER_HIDE_THRESHOLD;
    if (sticky !== headerStickyRef.current) {
      headerStickyRef.current = sticky;
      setHeaderSticky(sticky);
    }
  }, []);

  const handleEndReached = useCallback(() => {
    setData((prev) => [
      ...prev,
      ...Array.from({ length: 20 }, (_, index) => buildActivity(prev.length + index)),
    ]);
  }, []);

  // Pull-to-refresh: prepend a fresh batch after a short delay (stand-in fetch).
  const handleRefresh = useCallback(() => {
    setRefreshing(true);
    window.setTimeout(() => {
      setData((prev) => [
        ...Array.from({ length: 10 }, (_, index) => buildActivity(index)),
        ...prev,
      ]);
      setRefreshing(false);
    }, 1200);
  }, []);

  const header = useMemo(
    () => (
      <Activity.Header
        title="Activity"
        subtitle="Imperative scroll & reach thresholds, opens at index 30"
        actions={[
          { label: 'Offset 2000', onPress: () => shadowlistRef.current?.scrollToOffset(2000) },
          { label: 'Scroll to end', onPress: () => shadowlistRef.current?.scrollToEnd() },
          {
            label: `Start ×${startThreshold}`,
            onPress: () =>
              setStartThreshold((current) => nextInCycle(START_REACHED_THRESHOLDS, current)),
          },
          {
            label: `End ×${endThreshold}`,
            onPress: () =>
              setEndThreshold((current) => nextInCycle(END_REACHED_THRESHOLDS, current)),
          },
        ]}
      />
    ),
    [startThreshold, endThreshold]
  );

  const footer = useMemo(
    () => <ListFooter text={`Viewable: ${viewableLabel} · Total: ${data.length}`} />,
    [viewableLabel, data.length]
  );

  return (
    <div style={styles.container}>
      <Activity.List
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        keyExtractor={(item) => item.id}
        renderElement={({ element }) => <Activity.Row element={element} />}
        containerOffsetIndex={30}
        stickyHeader={headerSticky}
        refreshing={refreshing}
        onRefresh={handleRefresh}
        ListHeaderComponent={header}
        ListFooterComponent={footer}
        onScroll={handleScroll}
        onEndReached={handleEndReached}
        onStartReachedThreshold={startThreshold}
        onEndReachedThreshold={endThreshold}
        onViewableItemsChanged={handleViewableItemsChanged}
      />
    </div>
  );
};

const styles: Record<string, CSSProperties> = {
  container: {
    position: 'relative',
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
  list: {
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
};
