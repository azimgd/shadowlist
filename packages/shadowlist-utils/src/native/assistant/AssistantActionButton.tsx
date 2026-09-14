import type { ReactNode } from 'react';
import { Pressable, Text, StyleSheet } from 'react-native';
import { colors, typography, spacing, radius, fontWeight } from '../theme';

export interface AssistantActionButtonProps {
  // Accessibility label, and the visible text when `showLabel` is set.
  label: string;
  onPress: () => void;
  // The icon.
  children: ReactNode;
  showLabel?: boolean;
  disabled?: boolean;
  /*
   * A toggle that is currently on (feedback, a filter). Announced as the button's selected
   * state; without it a screen reader reads the accent colour's meaning as nothing at all.
   */
  selected?: boolean;
}

// Compact icon button for message actions (copy, retry, feedback, share, edit).
export const AssistantActionButton = ({
  label,
  onPress,
  children,
  showLabel = false,
  disabled = false,
  selected,
}: AssistantActionButtonProps) => (
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

const styles = StyleSheet.create({
  button: {
    minWidth: 32,
    height: 32,
    borderRadius: radius.sm,
    alignItems: 'center',
    justifyContent: 'center',
  },
  buttonLabeled: {
    flexDirection: 'row',
    gap: spacing.xs,
    paddingHorizontal: 10,
    backgroundColor: colors.elevated,
  },
  label: {
    color: colors.label,
    ...typography.footnote,
    fontWeight: fontWeight.semibold,
  },
  dimmed: {
    opacity: 0.35,
  },
});
