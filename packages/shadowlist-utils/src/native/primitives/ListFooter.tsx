import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { colors, typography, spacing } from '../theme';

export interface ListFooterProps {
  text?: string;
}

export const ListFooter = memo(({ text = 'End of list' }: ListFooterProps) => {
  return (
    <View style={styles.container}>
      <Text style={styles.text}>{text}</Text>
    </View>
  );
});

const styles = StyleSheet.create({
  container: {
    backgroundColor: colors.background,
    paddingHorizontal: spacing.lg,
    paddingVertical: spacing.xl,
    alignItems: 'center',
  },
  text: {
    color: colors.secondaryLabel,
    ...typography.footnote,
  },
});
