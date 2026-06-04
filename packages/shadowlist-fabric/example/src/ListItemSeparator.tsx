import { memo } from 'react';
import { View, StyleSheet } from 'react-native';

export const ListItemSeparator = memo(() => {
  return (
    <View style={styles.container}>
      <View style={styles.line} />
    </View>
  );
});

const styles = StyleSheet.create({
  // Opaque full-width row so nothing shows through behind the inset divider.
  container: {
    backgroundColor: '#000000',
  },
  line: {
    height: StyleSheet.hairlineWidth,
    backgroundColor: '#2F3336',
    marginLeft: 64,
  },
});
