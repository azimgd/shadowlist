import { memo, useEffect } from 'react';
import { View, StyleSheet } from 'react-native';
import Animated, {
  Easing,
  cancelAnimation,
  useAnimatedStyle,
  useSharedValue,
  withDelay,
  withRepeat,
  withTiming,
} from 'react-native-reanimated';
import { colors, spacing } from '../theme';

const PULSE_MS = 600;
// Stagger between the three typing dots.
const TYPING_STAGGER_MS = 160;

/*
 * Opacity that breathes between 0.25 and 1 for as long as the caller is mounted. Runs on
 * the UI thread, so a streaming row that re-renders every flush never restarts it.
 */
export function usePulseStyle(delay = 0) {
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

// The streaming cursor: one breathing dot that trails the text while it arrives.
export const PulsingDot = memo(
  ({ size = 8, color = colors.label, delay = 0 }: PulsingDotProps) => {
    const pulse = usePulseStyle(delay);
    return (
      <Animated.View
        style={[
          { width: size, height: size, borderRadius: size / 2 },
          { backgroundColor: color },
          pulse,
        ]}
      />
    );
  }
);

// Three staggered dots, shown before the first token of a reply has arrived.
export const AssistantTypingIndicator = memo(() => {
  return (
    <View
      style={styles.typing}
      // Focusable with a role, or the label below belongs to nothing and is never read.
      accessible
      accessibilityRole="progressbar"
      accessibilityLabel="Assistant is typing"
    >
      <PulsingDot size={7} color={colors.secondaryLabel} />
      <PulsingDot
        size={7}
        color={colors.secondaryLabel}
        delay={TYPING_STAGGER_MS}
      />
      <PulsingDot
        size={7}
        color={colors.secondaryLabel}
        delay={TYPING_STAGGER_MS * 2}
      />
    </View>
  );
});

const styles = StyleSheet.create({
  typing: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.xs,
    paddingVertical: spacing.sm,
  },
});
