import { memo, useEffect } from 'react';
import { Pressable, StyleSheet } from 'react-native';
import Animated, {
  useAnimatedStyle,
  useSharedValue,
  withTiming,
} from 'react-native-reanimated';
import { colors, spacing, radius } from '../theme';
import { Chevron } from '../icons';

export interface AssistantScrollButtonProps {
  visible: boolean;
  onPress: () => void;
}

const FADE_MS = 180;

/*
 * Floating "jump to latest" button, centered over the bottom edge of its parent. Place it
 * as a sibling after the list. It fades and scales rather than mounting, so showing it
 * never shifts layout, and it ignores touches while hidden.
 */
export const AssistantScrollButton = memo(
  ({ visible, onPress }: AssistantScrollButtonProps) => {
    const progress = useSharedValue(visible ? 1 : 0);

    useEffect(() => {
      progress.value = withTiming(visible ? 1 : 0, { duration: FADE_MS });
    }, [visible, progress]);

    const animatedStyle = useAnimatedStyle(() => ({
      opacity: progress.value,
      transform: [{ scale: 0.85 + progress.value * 0.15 }],
    }));

    return (
      <Animated.View
        style={[
          styles.container,
          visible ? styles.interactive : styles.inert,
          animatedStyle,
        ]}
        /*
         * Faded out it is still in the tree, so a screen reader would otherwise offer
         * "Scroll to latest" on a button nobody can see or press.
         */
        accessibilityElementsHidden={!visible}
        importantForAccessibility={visible ? 'auto' : 'no-hide-descendants'}
      >
        <Pressable
          onPress={onPress}
          hitSlop={8}
          accessibilityRole="button"
          accessibilityLabel="Scroll to latest"
          style={({ pressed }) => [styles.button, pressed && styles.pressed]}
        >
          <Chevron
            direction="down"
            size={18}
            color={colors.label}
            strokeWidth={2.2}
          />
        </Pressable>
      </Animated.View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    position: 'absolute',
    left: 0,
    right: 0,
    bottom: spacing.md,
    alignItems: 'center',
  },
  // Only the button takes touches; the full-width row around it passes them through.
  interactive: {
    pointerEvents: 'box-none',
  },
  // Faded out: ignore touches entirely so the list underneath stays scrollable.
  inert: {
    pointerEvents: 'none',
  },
  button: {
    width: 36,
    height: 36,
    borderRadius: radius.pill,
    backgroundColor: colors.elevated2,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: colors.separator,
    alignItems: 'center',
    justifyContent: 'center',
  },
  pressed: {
    opacity: 0.6,
  },
});
