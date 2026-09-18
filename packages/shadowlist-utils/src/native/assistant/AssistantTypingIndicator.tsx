import { memo, useEffect } from 'react';
import { View, StyleSheet, type StyleProp, type ViewStyle } from 'react-native';
import Animated, {
  Easing,
  cancelAnimation,
  useAnimatedStyle,
  useSharedValue,
  withDelay,
  withRepeat,
  withTiming,
} from 'react-native-reanimated';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { defaultAssistantLabels, type AssistantLabels } from './labels';

const PULSE_MS = 600;
const TYPING_STAGGER_MS = 160;

export interface AssistantTypingIndicatorProps {
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

/*
 * Opacity that breathes between 0.25 and 1 for as long as the caller is mounted. Runs on
 * the UI thread, so a streaming row that re-renders every flush never restarts it.
 */
export function usePulseStyle(
  delay = 0
): ReturnType<typeof useAnimatedStyle<{ opacity: number }>> {
  const progress = useSharedValue(0);

  useEffect(() => {
    progress.value = withDelay(
      delay,
      withRepeat(
        withTiming(1, {
          duration: PULSE_MS,
          easing: Easing.inOut(Easing.quad),
        }),
        -1,
        true
      )
    );
    return () => cancelAnimation(progress);
  }, [delay, progress]);

  return useAnimatedStyle(() => ({ opacity: 0.25 + progress.value * 0.75 }));
}

interface PulsingDotProps {
  size?: number;
  color?: string;
  delay?: number;
}

export const PulsingDot = memo(
  ({ size = 8, color, delay = 0 }: PulsingDotProps) => {
    const theme = useTheme();
    const pulse = usePulseStyle(delay);
    return (
      <Animated.View
        style={[
          { width: size, height: size, borderRadius: size / 2 },
          { backgroundColor: color ?? theme.colors.label },
          pulse,
        ]}
      />
    );
  }
);

export const AssistantTypingIndicator = memo(
  ({ labels, style }: AssistantTypingIndicatorProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const l = useLabels(defaultAssistantLabels, labels);
    const color = theme.colors.secondaryLabel;
    return (
      <View
        style={[styles.typing, style]}
        // Focusable with a role, or the label below belongs to nothing and is never read.
        accessible
        accessibilityRole="progressbar"
        accessibilityLabel={l.typing}
      >
        <PulsingDot size={7} color={color} />
        <PulsingDot size={7} color={color} delay={TYPING_STAGGER_MS} />
        <PulsingDot size={7} color={color} delay={TYPING_STAGGER_MS * 2} />
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    typing: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.xs,
      paddingVertical: theme.spacing.sm,
    },
  })
);
