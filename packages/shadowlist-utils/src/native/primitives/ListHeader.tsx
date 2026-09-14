import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { colors, typography, spacing } from '../theme';

export interface ListHeaderProps {
  title?: string;
  subtitle?: string;
}

/*
 * iOS large-title header. No divider; the list separators below carry the
 * visual break, matching the expanded large-title state.
 */
export const ListHeader = memo(
  ({ title = 'Header', subtitle }: ListHeaderProps) => {
    return (
      <View style={styles.container}>
        <Text style={styles.title}>{title}</Text>
        {subtitle ? <Text style={styles.subtitle}>{subtitle}</Text> : null}
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    backgroundColor: colors.background,
    paddingHorizontal: spacing.lg,
    paddingTop: spacing.xs,
    paddingBottom: spacing.md,
  },
  title: {
    color: colors.label,
    ...typography.largeTitle,
  },
  subtitle: {
    color: colors.secondaryLabel,
    ...typography.subhead,
    marginTop: spacing.xxs,
  },
});
