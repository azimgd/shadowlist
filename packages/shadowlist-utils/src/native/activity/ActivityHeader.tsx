import { memo, type ReactNode } from 'react';
import {
  View,
  Text,
  Pressable,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles } from '../theme';

export interface ActivityHeaderAction {
  label: string;
  onPress: () => void;
  icon?: ReactNode;
  accessibilityLabel?: string;
}

export interface ActivityHeaderProps {
  title: string;
  subtitle?: string;
  actions?: ReadonlyArray<ActivityHeaderAction>;
  style?: StyleProp<ViewStyle>;
}

const ActionButton = ({ action }: { action: ActivityHeaderAction }) => {
  const styles = useStyles();
  return (
    <Pressable
      onPress={action.onPress}
      accessibilityRole="button"
      accessibilityLabel={action.accessibilityLabel ?? action.label}
      style={({ pressed }) => [styles.action, pressed && styles.actionPressed]}
    >
      {action.icon}
      <Text style={styles.actionText}>{action.label}</Text>
    </Pressable>
  );
};

export const ActivityHeader = memo(
  ({ title, subtitle, actions, style }: ActivityHeaderProps) => {
    const styles = useStyles();
    return (
      <View style={[styles.container, style]}>
        <Text style={styles.title} accessibilityRole="header">
          {title}
        </Text>
        {subtitle ? <Text style={styles.subtitle}>{subtitle}</Text> : null}
        {actions && actions.length > 0 ? (
          <View style={styles.actions}>
            {actions.map((action, index) => (
              // Keyed by position, so a label can change, like a counter, without a remount.
              <ActionButton key={index} action={action} />
            ))}
          </View>
        ) : null}
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
    actions: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: theme.spacing.sm,
      marginTop: 14,
    },
    action: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.xs,
      backgroundColor: theme.colors.accentSoft,
      borderRadius: theme.radius.sm,
      paddingHorizontal: 14,
      paddingVertical: theme.spacing.sm,
    },
    actionPressed: {
      opacity: 0.6,
    },
    actionText: {
      color: theme.colors.accent,
      ...theme.typography.footnote,
      fontWeight: theme.fontWeight.semibold,
    },
  })
);
