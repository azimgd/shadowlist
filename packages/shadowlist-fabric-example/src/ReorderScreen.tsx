import { useCallback, useMemo } from 'react';
import { View, StyleSheet } from 'react-native';
import { Reorder, ListHeader, colors } from 'shadowlist-utils/native';
import {
  generateContact,
  useListController,
  type ContactItem,
} from 'shadowlist-utils';

export const ReorderScreen = () => {
  const initialData = useMemo(
    () => Array.from({ length: 80 }, (_, index) => generateContact(index)),
    []
  );
  const list = useListController<ContactItem>({ initialData });
  const { setData } = list;

  const handleReorder = useCallback(
    ({ data: reordered }: { data: ContactItem[] }) => setData(reordered),
    [setData]
  );

  const renderElement = useCallback(
    ({ element }: { element: ContactItem }) => (
      <Reorder.Row element={element} />
    ),
    []
  );

  return (
    <View style={styles.container}>
      <Reorder.List
        data={list.data}
        style={styles.list}
        onReorder={handleReorder}
        renderElement={renderElement}
        ListHeaderComponent={
          <ListHeader
            title="Reorder"
            subtitle="Press and hold a row, then drag to reorder"
          />
        }
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
