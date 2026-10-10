import { useEffect, useRef, type ReactNode } from 'react';
import {
  Animated,
  Easing,
  type StyleProp,
  type TextStyle,
  type ViewProps,
  type ViewStyle,
} from 'react-native';

const PULSE_MS = 600;

/*
 * The native Animated driver sets opacity on the view directly. A Reanimated loop here
 * committed the whole tree every frame, and with the dot inside a list row every commit
 * ran the list's layout pass too.
 */
function usePulseOpacity(delay = 0) {
  const progress = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    const pulse = Animated.sequence([
      Animated.delay(delay),
      Animated.loop(
        Animated.sequence([
          Animated.timing(progress, {
            toValue: 1,
            duration: PULSE_MS,
            easing: Easing.inOut(Easing.quad),
            useNativeDriver: true,
          }),
          Animated.timing(progress, {
            toValue: 0,
            duration: PULSE_MS,
            easing: Easing.inOut(Easing.quad),
            useNativeDriver: true,
          }),
        ])
      ),
    ]);
    pulse.start();
    return () => pulse.stop();
  }, [delay, progress]);

  return progress.interpolate({ inputRange: [0, 1], outputRange: [0.25, 1] });
}

export const PulseView = ({
  delay,
  style,
}: {
  delay?: number;
  style?: StyleProp<ViewStyle>;
}) => {
  const opacity = usePulseOpacity(delay);
  return <Animated.View style={[style, { opacity }]} />;
};

export const PulseText = ({
  style,
  children,
}: {
  style?: StyleProp<TextStyle>;
  children?: ReactNode;
}) => {
  const opacity = usePulseOpacity();
  return <Animated.Text style={[style, { opacity }]}>{children}</Animated.Text>;
};

export type FadeScaleViewProps = ViewProps & {
  visible: boolean;
  duration: number;
};

export const FadeScaleView = ({
  visible,
  duration,
  style,
  ...props
}: FadeScaleViewProps) => {
  const progress = useRef(new Animated.Value(visible ? 1 : 0)).current;

  useEffect(() => {
    const fade = Animated.timing(progress, {
      toValue: visible ? 1 : 0,
      duration,
      useNativeDriver: true,
    });
    fade.start();
    return () => fade.stop();
  }, [visible, duration, progress]);

  const scale = progress.interpolate({
    inputRange: [0, 1],
    outputRange: [0.85, 1],
  });

  return (
    <Animated.View
      {...props}
      style={[style, { opacity: progress, transform: [{ scale }] }]}
    />
  );
};
