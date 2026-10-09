import { memo } from 'react';
import {
  View,
  StyleSheet,
  type ColorValue,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { defaultAssistantLabels, type AssistantLabels } from './labels';
import { PulseView } from './motion';

const TYPING_STAGGER_MS = 160;

export interface AssistantTypingIndicatorProps {
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

interface PulsingDotProps {
  size?: number;
  color?: ColorValue;
  delay?: number;
}

export const PulsingDot = memo(
  ({ size = 8, color, delay = 0 }: PulsingDotProps) => {
    const theme = useTheme();
    return (
      <PulseView
        delay={delay}
        style={[
          { width: size, height: size, borderRadius: size / 2 },
          { backgroundColor: color ?? theme.colors.label },
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
        // Needs focus and a role, or screen readers never read the label below.
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
