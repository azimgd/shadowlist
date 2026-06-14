import { useCallback, useState, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { type ShadowlistCommands } from 'shadowlist';
import {
  Masonry,
  ListHeader,
  ListFooter,
  colors,
} from 'shadowlist-utils/native';
import { generateMasonryElement, type MasonryItem } from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

export const MasonryScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<MasonryItem[]>(() =>
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

  const renderElement = useCallback(
    ({ element }: { element: MasonryItem }) => (
      <Masonry.Card element={element} />
    ),
    []
  );

  return (
    <View style={styles.container}>
      <Masonry.List
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        columns={3}
        renderElement={renderElement}
        ListHeaderComponent={
          <ListHeader title="Masonry" subtitle="Three column grid layout" />
        }
        ListFooterComponent={<ListFooter text="End of masonry grid" />}
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
