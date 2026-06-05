import { useState, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist';
import { FloatingActionBar } from './FloatingActionBar';
import {
  MasonryElement,
  type MasonryElement as MasonryElementType,
} from './MasonryElement';
import { HeaderListItem } from './HeaderListItem';
import { FooterListItem } from './FooterListItem';
import { generateMasonryElement } from './constants';

export const MasonryScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<MasonryElementType[]>(() =>
    Array.from({ length: 100 }, (_, index) => generateMasonryElement(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateMasonryElement(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateMasonryElement(currentLength + index)
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
        columns={3}
        renderElement={({ element, index }) => (
          <MasonryElement element={element} index={index} />
        )}
        ListHeaderComponent={
          <HeaderListItem title="Masonry" subtitle="Three column grid layout" />
        }
        ListFooterComponent={<FooterListItem text="End of masonry grid" />}
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
