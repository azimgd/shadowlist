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
 * onClear shows a × after the label that clears only this chip's own state. A selected chip is
 * filled with the accent: it is the one switched on. clearAccessibilityLabel defaults to
 * "Clear <label>".
 */
export interface ChipProps {
  label: string;
  onPress?: () => void;
  onClear?: () => void;
  selected?: boolean;
  variant?: 'tinted' | 'plain';
  size?: 'regular' | 'compact';
  mono?: boolean;
  maxFontSizeMultiplier?: number;
  accessibilityLabel?: string;
  clearAccessibilityLabel?: string;
  style?: StyleProp<ViewStyle>;
}

export const Chip = memo(
  ({
    label,
    onPress,
    onClear,
    selected = false,
    variant = 'tinted',
    size = 'regular',
    mono = false,
    maxFontSizeMultiplier,
    accessibilityLabel,
    clearAccessibilityLabel,
    style,
  }: ChipProps) => {
    const styles = useChipStyles();
    const plain = variant === 'plain' && !selected;
    const compact = size === 'compact';
    return (
      <Pressable
        accessibilityRole="button"
        accessibilityLabel={accessibilityLabel ?? label}
        accessibilityState={{ selected }}
        onPress={onPress}
        style={({ pressed }) => [
          styles.chip,
          compact && styles.chipCompact,
          plain && styles.chipPlain,
          selected && styles.chipSelected,
          pressed && onPress !== undefined && styles.pressed,
          style,
        ]}
      >
        <Text
          style={[
            styles.label,
            compact && styles.labelCompact,
            plain && styles.labelPlain,
            selected && styles.labelSelected,
            mono && styles.labelMono,
          ]}
          numberOfLines={1}
          maxFontSizeMultiplier={maxFontSizeMultiplier}
        >
          {label}
        </Text>
        {onClear !== undefined ? (
          <Pressable
            accessibilityRole="button"
            accessibilityLabel={clearAccessibilityLabel ?? `Clear ${label}`}
            hitSlop={8}
            onPress={onClear}
          >
            <Text
              style={[
                styles.clear,
                plain && styles.labelPlain,
                selected && styles.labelSelected,
              ]}
            >
              ×
            </Text>
          </Pressable>
        ) : null}
      </Pressable>
    );
  }
);

/*
 * ChoiceChip shares these.
 */
export const useChipStyles = createStyles(({ colors, typography, fonts }) =>
  StyleSheet.create({
    chip: {
      flexDirection: 'row',
      alignItems: 'center',
      justifyContent: 'center',
      gap: 4,
      height: 34,
      maxWidth: 240,
      paddingHorizontal: 14,
      borderRadius: 17,
      backgroundColor: colors.accentSoft,
    },
    chipCompact: {
      gap: 6,
      height: 28,
      paddingHorizontal: 11,
      borderRadius: 14,
    },
    chipPlain: {
      backgroundColor: colors.fill,
    },
    chipSelected: {
      backgroundColor: colors.accent,
    },
    label: {
      ...typography.subhead,
      fontWeight: '500',
      color: colors.accent,
    },
    labelCompact: {
      ...typography.footnote,
      fontWeight: '500',
    },
    labelSelected: {
      color: colors.onAccent,
    },
    clear: {
      fontSize: 15,
      color: colors.accent,
      opacity: 0.7,
    },
    labelPlain: {
      color: colors.secondaryLabel,
    },
    labelMono: {
      fontFamily: fonts.mono,
      letterSpacing: -0.6,
    },
    pressed: {
      opacity: 0.55,
    },
  })
);
