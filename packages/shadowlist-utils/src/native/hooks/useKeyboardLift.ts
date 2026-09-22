import { useMemo } from 'react';
import type { Animated } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { useKeyboardAnimation } from 'shadowlist';

export interface UseKeyboardLiftOptions {
  gap?: number;
}

/*
 * Moves a list and composer column up with the keyboard. The keyboard height includes the
 * bottom safe area the composer already pads for, so the lift stays at 0 until the keyboard
 * passes it, then settles gap above the keyboard.
 */
export function useKeyboardLift({
  gap = 8,
}: UseKeyboardLiftOptions = {}): Animated.AnimatedInterpolation<number> {
  const insets = useSafeAreaInsets();
  const { height } = useKeyboardAnimation();

  return useMemo(() => {
    const safe = insets.bottom;
    return height.interpolate({
      inputRange: safe > 0 ? [0, safe, safe + 1] : [0, 1, 2],
      outputRange: [0, -gap, -gap - 1],
    });
  }, [height, insets.bottom, gap]);
}
