import { useState } from 'react';
import { View, StyleSheet } from 'react-native';
import { Reorder, ListHeader, colors } from 'shadowlist-utils/native';
import { generateContact, type ContactItem } from 'shadowlist-utils';

export const ReorderScreen = () => {
  const [data, setData] = useState<ContactItem[]>(() =>
    Array.from({ length: 80 }, (_, index) => generateContact(index))
  );

  return (
    <View style={styles.container}>
      <Reorder.List
        data={data}
        style={styles.list}
        onReorder={({ data: reordered }) => setData(reordered)}
        renderElement={({ element }) => <Reorder.Row element={element} />}
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
