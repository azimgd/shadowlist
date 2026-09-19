import { memo } from 'react';
import { View, StyleSheet, type StyleProp, type ViewStyle } from 'react-native';
import { createStyles } from '../theme';

export interface ItemSeparatorProps {
  style?: StyleProp<ViewStyle>;
}

export const ItemSeparator = memo(({ style }: ItemSeparatorProps) => {
  const styles = useStyles();
  return (
    <View style={[styles.container, style]}>
      <View style={styles.line} />
    </View>
  );
});

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      backgroundColor: theme.colors.background,
    },
    line: {
      height: StyleSheet.hairlineWidth,
      backgroundColor: theme.colors.separator,
      marginLeft: theme.rowInset,
    },
  })
);
