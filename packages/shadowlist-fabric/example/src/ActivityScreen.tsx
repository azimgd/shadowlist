import { useState, useRef, useCallback, useMemo } from 'react';
import { View, StyleSheet } from 'react-native';
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

  // append more rows as the bottom edge is reached.
  const handleEndReached = useCallback(() => {
    setData((prev) => [
      ...prev,
      ...Array.from({ length: 20 }, (_, index) =>
        buildActivity(prev.length + index)
      ),
    ]);
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
    () => <FooterListItem text={`Viewable indices: ${viewableLabel}`} />,
    [viewableLabel]
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
});
