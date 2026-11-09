import { memo } from 'react';
import {
  View,
  Text,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles } from '../theme';

export interface ListHeaderProps {
  title: string;
  subtitle?: string;
  style?: StyleProp<ViewStyle>;
}

export const ListHeader = memo(
  ({ title, subtitle, style }: ListHeaderProps) => {
    const styles = useStyles();
    return (
      <View style={[styles.container, style]}>
        <Text style={styles.title} accessibilityRole="header">
          {title}
        </Text>
        {subtitle ? <Text style={styles.subtitle}>{subtitle}</Text> : null}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      backgroundColor: theme.colors.background,
      paddingHorizontal: theme.spacing.lg,
      paddingTop: theme.spacing.xs,
      paddingBottom: theme.spacing.md,
    },
    title: {
      color: theme.colors.label,
      ...theme.typography.largeTitle,
    },
    subtitle: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.subhead,
      marginTop: theme.spacing.xxs,
    },
  })
);
