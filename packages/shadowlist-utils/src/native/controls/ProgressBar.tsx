import { memo } from 'react';
import {
  StyleSheet,
  View,
  type ColorValue,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles, useTheme } from '../theme';

export interface ProgressBarProps {
  fraction: number;
  accessibilityLabel: string;
  color?: ColorValue;
  height?: number;
  style?: StyleProp<ViewStyle>;
}

export const ProgressBar = memo(
  ({
    fraction,
    accessibilityLabel,
    color,
    height = 6,
    style,
  }: ProgressBarProps) => {
    const styles = useStyles();
    const { colors } = useTheme();
    const clamped = clampFraction(fraction);
    const shape = { height, borderRadius: height / 2 };
    return (
      <View
        style={[styles.track, shape, style]}
        accessibilityRole="progressbar"
        accessibilityLabel={accessibilityLabel}
        accessibilityValue={{
          min: 0,
          max: 100,
          now: Math.round(clamped * 100),
        }}
      >
        <View
          style={[
            shape,
            {
              width: `${clamped * 100}%`,
              backgroundColor: color ?? colors.accent,
            },
          ]}
        />
      </View>
    );
  }
);

export function clampFraction(fraction: number): number {
  return Math.max(0, Math.min(1, Number.isFinite(fraction) ? fraction : 0));
}

const useStyles = createStyles(({ colors }) =>
  StyleSheet.create({
    track: {
      backgroundColor: colors.fill,
      overflow: 'hidden',
    },
  })
);
