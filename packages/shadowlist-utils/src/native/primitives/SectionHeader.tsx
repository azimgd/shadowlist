import { memo } from 'react';
import {
  View,
  Text,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles } from '../theme';

export interface SectionHeaderProps {
  title: string;
  count?: number;
  style?: StyleProp<ViewStyle>;
}

export const SectionHeader = memo(
  ({ title, count, style }: SectionHeaderProps) => {
    const styles = useStyles();
    return (
      <View style={[styles.container, style]}>
        <Text style={styles.title} accessibilityRole="header">
          {title}
        </Text>
        {count !== undefined ? <Text style={styles.count}>{count}</Text> : null}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      backgroundColor: theme.colors.elevated,
      paddingHorizontal: theme.spacing.lg,
      paddingVertical: 10,
      flexDirection: 'row',
      alignItems: 'flex-end',
      justifyContent: 'space-between',
    },
    title: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.footnote,
      fontWeight: theme.fontWeight.semibold,
      textTransform: 'uppercase',
    },
    count: {
      color: theme.colors.tertiaryLabel,
      ...theme.typography.footnote,
    },
  })
);
