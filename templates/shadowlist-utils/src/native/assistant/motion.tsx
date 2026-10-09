import { useEffect, type ReactNode } from 'react';
import type { StyleProp, TextStyle, ViewProps, ViewStyle } from 'react-native';
import Animated, {
  Easing,
  cancelAnimation,
  useAnimatedStyle,
  useSharedValue,
  withDelay,
  withRepeat,
  withTiming,
} from 'react-native-reanimated';

const PULSE_MS = 600;

/*
 * Fades opacity between 0.25 and 1 while mounted. It runs on the UI thread. A streaming
 * row that re-renders on every flush never restarts it.
 */
function usePulseStyle(delay = 0) {
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

export const PulseView = ({
  delay,
  style,
}: {
  delay?: number;
  style?: StyleProp<ViewStyle>;
}) => {
  const pulse = usePulseStyle(delay);
  return <Animated.View style={[style, pulse]} />;
};

export const PulseText = ({
  style,
  children,
}: {
  style?: StyleProp<TextStyle>;
  children?: ReactNode;
}) => {
  const pulse = usePulseStyle();
  return <Animated.Text style={[style, pulse]}>{children}</Animated.Text>;
};

export type FadeScaleViewProps = ViewProps & {
  visible: boolean;
  duration: number;
};

/*
 * Fades and scales in when visible and out when not. It stays mounted either way.
 */
export const FadeScaleView = ({
  visible,
  duration,
  style,
  ...props
}: FadeScaleViewProps) => {
  const progress = useSharedValue(visible ? 1 : 0);

  useEffect(() => {
    progress.value = withTiming(visible ? 1 : 0, { duration });
  }, [visible, duration, progress]);

  const animatedStyle = useAnimatedStyle(() => ({
    opacity: progress.value,
    transform: [{ scale: 0.85 + progress.value * 0.15 }],
  }));

  return <Animated.View {...props} style={[style, animatedStyle]} />;
};
