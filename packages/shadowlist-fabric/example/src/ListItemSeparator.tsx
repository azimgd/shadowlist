import { memo } from 'react';
import { View, StyleSheet } from 'react-native';
import { colors, ROW_INSET } from './theme';

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
    backgroundColor: colors.background,
  },
  line: {
    height: StyleSheet.hairlineWidth,
    backgroundColor: colors.separator,
    marginLeft: ROW_INSET,
  },
});
