import { memo, type ReactNode } from 'react';
import {
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles } from '../theme';

export interface EmptyStateProps {
  icon?: ReactNode;
  title: string;
  message?: string;
  style?: StyleProp<ViewStyle>;
}

// What a screen says before it has anything to show. It is read as one element.
export const EmptyState = memo(
  ({ icon, title, message, style }: EmptyStateProps) => {
    const styles = useStyles();
    return (
      <View
        style={[styles.container, style]}
        accessible
        accessibilityLabel={
          message === undefined ? title : `${title}. ${message}`
        }
      >
        {icon}
        <Text style={styles.title}>{title}</Text>
        {message !== undefined ? (
          <Text style={styles.message}>{message}</Text>
        ) : null}
      </View>
    );
  }
);

const useStyles = createStyles(({ colors, typography, spacing, grouped }) =>
  StyleSheet.create({
    container: {
      alignItems: 'center',
      gap: spacing.xs,
      paddingVertical: 28,
      paddingHorizontal: grouped.inset,
    },
    title: {
      ...typography.body,
      fontWeight: '500',
      color: colors.label,
      marginTop: 6,
    },
    message: {
      ...typography.footnote,
      color: colors.secondaryLabel,
      textAlign: 'center',
    },
  })
);
