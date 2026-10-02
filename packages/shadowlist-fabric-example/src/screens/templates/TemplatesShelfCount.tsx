import { StyleSheet, Text, View } from 'react-native';
import { createStyles } from 'shadowlist-utils/native';

export interface TemplatesShelfCountProps {
  count: number;
}

// A count bubble, the second row kind of the Templates page's TemplateList.
export const TemplatesShelfCount = ({ count }: TemplatesShelfCountProps) => {
  const styles = useStyles();
  return (
    <View style={styles.bubble}>
      <Text style={styles.text}>{count}</Text>
    </View>
  );
};

const useStyles = createStyles(({ colors, typography, spacing }) =>
  StyleSheet.create({
    bubble: {
      height: 34,
      minWidth: 34,
      paddingHorizontal: spacing.sm,
      borderRadius: 17,
      alignItems: 'center',
      justifyContent: 'center',
      backgroundColor: colors.elevated2,
    },
    text: {
      ...typography.subhead,
      color: colors.label,
      fontVariant: ['tabular-nums'],
    },
  })
);
