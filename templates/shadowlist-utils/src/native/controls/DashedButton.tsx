import { memo } from 'react';
import {
  Pressable,
  StyleSheet,
  Text,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles } from '../theme';

export interface DashedButtonProps {
  label: string;
  onPress: () => void;
  disabled?: boolean;
  style?: StyleProp<ViewStyle>;
}

/*
 * A full-width dashed outline that adds one more of something: "Add sort".
 */
export const DashedButton = memo(
  ({ label, onPress, disabled = false, style }: DashedButtonProps) => {
    const styles = useStyles();
    return (
      <Pressable
        accessibilityRole="button"
        accessibilityLabel={label}
        accessibilityState={{ disabled }}
        disabled={disabled}
        onPress={onPress}
        style={({ pressed }) => [
          styles.button,
          pressed && styles.pressed,
          disabled && styles.disabled,
          style,
        ]}
      >
        <Text style={styles.label}>{label}</Text>
      </Pressable>
    );
  }
);

const useStyles = createStyles(({ colors, typography, tapTarget }) =>
  StyleSheet.create({
    button: {
      height: tapTarget,
      borderRadius: 10,
      borderWidth: 1,
      borderStyle: 'dashed',
      borderColor: colors.accent,
      backgroundColor: colors.accentSoft,
      alignItems: 'center',
      justifyContent: 'center',
    },
    label: {
      ...typography.body,
      fontWeight: '500',
      color: colors.accent,
    },
    pressed: {
      opacity: 0.55,
    },
    disabled: {
      opacity: 0.4,
    },
  })
);
