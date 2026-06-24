import { useCallback, useMemo, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import {
  Masonry,
  ListHeader,
  ListFooter,
  colors,
} from 'shadowlist-utils/native';
import {
  generateMasonryElement,
  useListController,
  type MasonryItem,
} from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

export const MasonryScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const initialData = useMemo(
    () =>
      Array.from({ length: 100 }, (_, index) => generateMasonryElement(index)),
    []
  );
  const list = useListController<MasonryItem>({ initialData });

  const handlePrepend = () =>
    list.prepend(
      Array.from({ length: 10 }, (_, index) =>
        generateMasonryElement(list.data.length + index)
      )
    );
  const handleAppend = () =>
    list.append(
      Array.from({ length: 10 }, (_, index) =>
        generateMasonryElement(list.data.length + index)
      )
    );
  const handleScrollToRandom = () =>
    shadowlistRef.current?.scrollToIndex(
      Math.floor(Math.random() * list.data.length)
    );

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
        data={list.data}
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
