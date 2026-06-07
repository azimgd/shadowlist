import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { colors, typography } from '../theme';

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
    paddingHorizontal: 16,
    paddingVertical: 20,
    alignItems: 'center',
  },
  text: {
    color: colors.secondaryLabel,
    ...typography.footnote,
  },
});
