import { View, StyleSheet } from 'react-native';
import { Snap, colors } from 'shadowlist-utils/native';
import { generateSnapElement } from 'shadowlist-utils';

const data = Array.from({ length: 50 }, (_, index) =>
  generateSnapElement(index)
);

export const SnapScreen = () => {
  return (
    <View style={styles.container}>
      <Snap.List
        data={data}
        style={styles.list}
        keyExtractor={(item) => item.id}
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
