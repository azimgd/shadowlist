import { useCallback, useMemo, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import {
  Nested,
  ListHeader,
  ListFooter,
  colors,
} from 'shadowlist-utils/native';
import {
  generateNestedElement,
  useListController,
  type NestedItem,
} from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

export const NestedScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const initialData = useMemo(
    () =>
      Array.from({ length: 20 }, (_, index) => generateNestedElement(index)),
    []
  );
  const list = useListController<NestedItem>({ initialData });

  const handlePrepend = () =>
    list.prepend(
      Array.from({ length: 5 }, (_, index) =>
        generateNestedElement(list.data.length + index)
      )
    );
  const handleAppend = () =>
    list.append(
      Array.from({ length: 5 }, (_, index) =>
        generateNestedElement(list.data.length + index)
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
    ({ element }: { element: NestedItem }) => <Nested.Row element={element} />,
    []
  );

  return (
    <View style={styles.container}>
      <Nested.List
        data={list.data}
        ref={shadowlistRef}
        style={styles.list}
        renderElement={renderElement}
        ListHeaderComponent={
          <ListHeader title="Nested" subtitle="Nested horizontal lists" />
        }
        ListFooterComponent={<ListFooter text="End of nested list" />}
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
