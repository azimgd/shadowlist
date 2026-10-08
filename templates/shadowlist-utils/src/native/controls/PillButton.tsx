import { memo } from 'react';
import {
  Pressable,
  StyleSheet,
  Text,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles } from '../theme';

/*
 * primary is the filled accent pill for the one thing to do next. tinted is an accent label on
 * a fill, for an action on offer that is not the point. plain is the quiet grey of Cancel.
 */
export type PillButtonVariant = 'primary' | 'tinted' | 'plain' | 'destructive';

export interface PillButtonProps {
  label: string;
  onPress: () => void;
  variant?: PillButtonVariant;
  disabled?: boolean;
  style?: StyleProp<ViewStyle>;
}

export const PillButton = memo(
  ({
    label,
    onPress,
    variant = 'primary',
    disabled = false,
    style,
  }: PillButtonProps) => {
    const styles = useStyles();
    return (
      <Pressable
        accessibilityRole="button"
        accessibilityLabel={label}
        accessibilityState={{ disabled }}
        disabled={disabled}
        onPress={onPress}
        style={({ pressed }) => [
          styles.pill,
          variant === 'primary'
            ? styles.primary
            : variant === 'tinted'
              ? styles.tinted
              : styles.plain,
          pressed && styles.pressed,
          disabled && styles.disabled,
          style,
        ]}
      >
        <Text style={styles[`${variant}Label`]}>{label}</Text>
      </Pressable>
    );
  }
);

const useStyles = createStyles(({ colors, typography, fontWeight }) =>
  StyleSheet.create({
    pill: {
      height: 40,
      minWidth: 72,
      paddingHorizontal: 14,
      borderRadius: 20,
      alignItems: 'center',
      justifyContent: 'center',
    },
    primary: {
      backgroundColor: colors.accent,
    },
    tinted: {
      backgroundColor: colors.fill,
    },
    plain: {
      backgroundColor: colors.background,
    },
    primaryLabel: {
      ...typography.body,
      fontWeight: fontWeight.semibold,
      color: colors.onAccent,
    },
    tintedLabel: {
      ...typography.body,
      fontWeight: fontWeight.semibold,
      color: colors.accent,
    },
    plainLabel: {
      ...typography.body,
      color: colors.secondaryLabel,
    },
    destructiveLabel: {
      ...typography.body,
      color: colors.red,
    },
    pressed: {
      opacity: 0.55,
    },
    disabled: {
      opacity: 0.4,
    },
  })
);
