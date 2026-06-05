import { useState, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { Shadowlist, type ShadowlistCommands } from 'shadowlist';
import { FloatingActionBar } from './FloatingActionBar';
import {
  NestedElement,
  type NestedElement as NestedElementType,
} from './NestedElement';
import { HeaderListItem } from './HeaderListItem';
import { FooterListItem } from './FooterListItem';
import { generateNestedElement } from './constants';

export const NestedScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<NestedElementType[]>(() =>
    Array.from({ length: 20 }, (_, index) => generateNestedElement(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 5 }, (_, index) =>
      generateNestedElement(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 5 }, (_, index) =>
      generateNestedElement(currentLength + index)
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
          <NestedElement element={element} index={index} />
        )}
        ListHeaderComponent={
          <HeaderListItem title="Nested" subtitle="Nested horizontal lists" />
        }
        ListFooterComponent={<FooterListItem text="End of nested list" />}
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
    height: 300,
  },
});
