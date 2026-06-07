import { useState, useRef, useCallback, useMemo, type CSSProperties } from 'react';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist-wasm';
import type { OnScroll } from 'shadowlist-wasm';
import { ActivityElement } from './ActivityElement';
import { ActivityHeader } from './ActivityHeader';
import { FooterListItem } from './FooterListItem';
import { ListItemSeparator } from './ListItemSeparator';
import { colors } from './theme';
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

  const dataRef = useRef(data);
  dataRef.current = data;
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

  // Append more rows before the bottom edge is reached.
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

  const handleScrollToOffset = useCallback(
    () => shadowlistRef.current?.scrollToOffset(2000),
    []
  );
  const handleScrollToEnd = useCallback(() => shadowlistRef.current?.scrollToEnd(), []);
  const handleScrollToRandom = useCallback(
    () => shadowlistRef.current?.scrollToIndex(Math.floor(Math.random() * dataRef.current.length)),
    []
  );
  const handleCycleStartThreshold = useCallback(
    () => setStartThreshold((current) => nextInCycle(START_REACHED_THRESHOLDS, current)),
    []
  );
  const handleCycleEndThreshold = useCallback(
    () => setEndThreshold((current) => nextInCycle(END_REACHED_THRESHOLDS, current)),
    []
  );

  const header = useMemo(
    () => (
      <ActivityHeader
        startThreshold={startThreshold}
        endThreshold={endThreshold}
        onScrollToOffset={handleScrollToOffset}
        onScrollToEnd={handleScrollToEnd}
        onScrollToRandom={handleScrollToRandom}
        onCycleStartThreshold={handleCycleStartThreshold}
        onCycleEndThreshold={handleCycleEndThreshold}
      />
    ),
    [
      startThreshold,
      endThreshold,
      handleScrollToOffset,
      handleScrollToEnd,
      handleScrollToRandom,
      handleCycleStartThreshold,
      handleCycleEndThreshold,
    ]
  );

  const footer = useMemo(
    () => (
      <FooterListItem
        text={`Viewable: ${viewableLabel} · Total: ${data.length}`}
      />
    ),
    [viewableLabel, data.length]
  );

  return (
    <div style={styles.container}>
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        keyExtractor={(item) => item.id}
        renderElement={({ element }) => <ActivityElement element={element} />}
        containerOffsetIndex={30}
        stickyHeader={headerSticky}
        stickyFooter
        refreshing={refreshing}
        onRefresh={handleRefresh}
        ListHeaderComponent={header}
        ListFooterComponent={footer}
        ItemSeparatorComponent={<ListItemSeparator />}
        onScroll={handleScroll}
        onEndReached={handleEndReached}
        onStartReachedThreshold={startThreshold}
        onEndReachedThreshold={endThreshold}
        viewabilityConfig={{ itemVisiblePercentThreshold: 60 }}
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
