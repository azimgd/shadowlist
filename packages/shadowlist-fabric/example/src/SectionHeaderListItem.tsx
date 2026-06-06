import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { colors, typography } from './theme';

interface SectionHeaderListItemProps {
  title: string;
  count?: number;
}

// iOS grouped section header: uppercase secondary-label text on a subtle
// translucent bar. The pinned header is opaque so rows scroll cleanly under it.
export const SectionHeaderListItem = memo(
  ({ title, count }: SectionHeaderListItemProps) => {
    return (
      <View style={styles.container}>
        <Text style={styles.title}>{title}</Text>
        {count !== undefined ? <Text style={styles.count}>{count}</Text> : null}
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    backgroundColor: colors.elevated,
    paddingHorizontal: 16,
    paddingVertical: 10,
    flexDirection: 'row',
    alignItems: 'flex-end',
    justifyContent: 'space-between',
  },
  title: {
    color: colors.secondaryLabel,
    ...typography.footnote,
    fontWeight: '600',
    textTransform: 'uppercase',
  },
  count: {
    color: colors.tertiaryLabel,
    ...typography.footnote,
  },
});
