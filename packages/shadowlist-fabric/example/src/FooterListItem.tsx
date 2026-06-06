import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { colors, typography } from './theme';

interface FooterListItemProps {
  text?: string;
}

export const FooterListItem = memo(
  ({ text = 'End of list' }: FooterListItemProps) => {
    return (
      <View style={styles.container}>
        <Text style={styles.text}>{text}</Text>
      </View>
    );
  }
);

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
