import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { colors, typography } from './theme';

interface HeaderListItemProps {
  title?: string;
  subtitle?: string;
}

// iOS large-title header. No divider — the list separators below carry the
// visual break, matching the expanded large-title state.
export const HeaderListItem = memo(
  ({ title = 'Header', subtitle }: HeaderListItemProps) => {
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
    paddingHorizontal: 16,
    paddingTop: 4,
    paddingBottom: 12,
  },
  title: {
    color: colors.label,
    ...typography.largeTitle,
  },
  subtitle: {
    color: colors.secondaryLabel,
    ...typography.subhead,
    marginTop: 2,
  },
});
