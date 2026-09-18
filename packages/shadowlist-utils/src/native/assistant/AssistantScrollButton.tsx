import { memo, useEffect } from 'react';
import {
  Pressable,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import Animated, {
  useAnimatedStyle,
  useSharedValue,
  withTiming,
} from 'react-native-reanimated';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { ChevronIcon } from '../icons';
import { defaultAssistantLabels, type AssistantLabels } from './labels';

export interface AssistantScrollButtonProps {
  visible: boolean;
  onPress: () => void;
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

const FADE_MS = 180;

export const AssistantScrollButton = memo(
  ({ visible, onPress, labels, style }: AssistantScrollButtonProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const l = useLabels(defaultAssistantLabels, labels);
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
          style,
          animatedStyle,
        ]}
        // Faded out it is still in the tree; hide it so a screen reader doesn't offer it.
        accessibilityElementsHidden={!visible}
        importantForAccessibility={visible ? 'auto' : 'no-hide-descendants'}
      >
        <Pressable
          onPress={onPress}
          hitSlop={8}
          accessibilityRole="button"
          accessibilityLabel={l.scrollToLatest}
          style={({ pressed }) => [styles.button, pressed && styles.pressed]}
        >
          <ChevronIcon
            direction="down"
            size={18}
            color={theme.colors.label}
            strokeWidth={2.2}
          />
        </Pressable>
      </Animated.View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      position: 'absolute',
      left: 0,
      right: 0,
      bottom: theme.spacing.md,
      alignItems: 'center',
    },
    interactive: {
      pointerEvents: 'box-none',
    },
    inert: {
      pointerEvents: 'none',
    },
    button: {
      width: 36,
      height: 36,
      borderRadius: theme.radius.pill,
      backgroundColor: theme.colors.elevated2,
      borderWidth: StyleSheet.hairlineWidth,
      borderColor: theme.colors.separator,
      alignItems: 'center',
      justifyContent: 'center',
    },
    pressed: {
      opacity: 0.6,
    },
  })
);
