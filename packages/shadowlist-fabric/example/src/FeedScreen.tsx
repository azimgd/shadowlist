import { useState, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist';
import { FloatingActionBar } from './FloatingActionBar';
import {
  FeedElement,
  type FeedElement as FeedElementType,
} from './FeedElement';
import { HeaderListItem } from './HeaderListItem';
import { FooterListItem } from './FooterListItem';
import { generateFeedElement } from './constants';

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

  const handleScrollToIndex = (index: number) => {
    shadowlistRef.current?.scrollToIndex(index);
  };

  return (
    <View style={styles.container}>
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderElement={({ element, index }) => (
          <FeedElement element={element} index={index} />
        )}
        ListHeaderComponent={
          <HeaderListItem title="Feed" subtitle="Vertical scrolling list" />
        }
        ListFooterComponent={<FooterListItem text="End of feed" />}
      />
      <FloatingActionBar
        onPrepend={handlePrepend}
        onAppend={handleAppend}
        onScrollToIndex={handleScrollToIndex}
        dataLength={data.length}
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
