import { memo } from 'react';
import { ActivityIndicator, View, StyleSheet } from 'react-native';
import { colors, spacing } from '../theme';

// Indeterminate loading spinner for infinite-scroll / pull-to-refresh footers.
export const Spinner = memo(
  ({
    size = 20,
    color = colors.secondaryLabel,
  }: {
    size?: number;
    color?: string;
  }) => {
    return (
      <View style={styles.container}>
        <ActivityIndicator size={size} color={color} />
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    alignItems: 'center',
    justifyContent: 'center',
    padding: spacing.lg,
  },
});
