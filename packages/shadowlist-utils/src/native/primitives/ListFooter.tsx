import { memo } from 'react';
import {
  View,
  Text,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles } from '../theme';

export interface ListFooterProps {
  text: string;
  style?: StyleProp<ViewStyle>;
}

export const ListFooter = memo(({ text, style }: ListFooterProps) => {
  const styles = useStyles();
  return (
    <View style={[styles.container, style]}>
      <Text style={styles.text}>{text}</Text>
    </View>
  );
});

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      backgroundColor: theme.colors.background,
      paddingHorizontal: theme.spacing.lg,
      paddingVertical: theme.spacing.xl,
      alignItems: 'center',
    },
    text: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.footnote,
    },
  })
);
