import type { ReactNode } from 'react';
import { Pressable, Text, StyleSheet } from 'react-native';
import { createStyles } from '../theme';

interface AssistantActionButtonProps {
  label: string;
  onPress: () => void;
  children: ReactNode;
  showLabel?: boolean;
  disabled?: boolean;
  selected?: boolean;
}

export const AssistantActionButton = ({
  label,
  onPress,
  children,
  showLabel = false,
  disabled = false,
  selected,
}: AssistantActionButtonProps) => {
  const styles = useStyles();
  return (
    <Pressable
      onPress={onPress}
      disabled={disabled}
      hitSlop={6}
      accessibilityRole="button"
      accessibilityLabel={label}
      accessibilityState={{ disabled, selected }}
      style={({ pressed }) => [
        styles.button,
        showLabel && styles.buttonLabeled,
        (pressed || disabled) && styles.dimmed,
      ]}
    >
      {children}
      {showLabel ? <Text style={styles.label}>{label}</Text> : null}
    </Pressable>
  );
};

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    button: {
      minWidth: 32,
      height: 32,
      borderRadius: theme.radius.sm,
      alignItems: 'center',
      justifyContent: 'center',
    },
    buttonLabeled: {
      flexDirection: 'row',
      gap: theme.spacing.xs,
      paddingHorizontal: 10,
      backgroundColor: theme.colors.elevated,
    },
    label: {
      color: theme.colors.label,
      ...theme.typography.footnote,
      fontWeight: theme.fontWeight.semibold,
    },
    dimmed: {
      opacity: 0.35,
    },
  })
);
