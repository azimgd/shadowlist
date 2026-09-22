import { memo, useState } from 'react';
import {
  View,
  Text,
  Pressable,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import Animated from 'react-native-reanimated';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { ChevronIcon } from '../icons';
import { defaultAssistantLabels, type AssistantLabels } from './labels';
import { usePulseStyle } from './AssistantTypingIndicator';

export interface AssistantThinkingProps {
  thinking: string;
  thinkingMs?: number;
  active: boolean;
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

/*
 * How much reasoning the one line preview keeps. Layout measures the whole string before
 * cutting it to one line, so pass only the tail it shows.
 */
const PREVIEW_TAIL_CHARS = 160;

const PulsingLabel = ({ label }: { label: string }) => {
  const styles = useStyles();
  const pulse = usePulseStyle();
  return <Animated.Text style={[styles.label, pulse]}>{label}</Animated.Text>;
};

export const AssistantThinking = memo(
  ({
    thinking,
    thinkingMs = 0,
    active,
    labels,
    style,
  }: AssistantThinkingProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const l = useLabels(defaultAssistantLabels, labels);
    const [expanded, setExpanded] = useState(false);
    const seconds = Math.max(1, Math.round(thinkingMs / 1000));
    const label = active ? l.thinking : l.thoughtFor(seconds);

    return (
      <View style={[styles.container, style]}>
        <Pressable
          onPress={() => setExpanded((previous) => !previous)}
          hitSlop={10}
          accessibilityRole="button"
          accessibilityLabel={label}
          accessibilityState={{ expanded }}
          style={({ pressed }) => [styles.header, pressed && styles.pressed]}
        >
          {active ? (
            <PulsingLabel label={label} />
          ) : (
            <Text style={styles.label}>{label}</Text>
          )}
          <ChevronIcon
            direction={expanded ? 'down' : 'right'}
            size={14}
            color={theme.colors.secondaryLabel}
            strokeWidth={1.8}
          />
        </Pressable>

        {expanded ? (
          <View style={styles.body}>
            <Text style={styles.bodyText} selectable>
              {thinking}
            </Text>
          </View>
        ) : thinking ? (
          /*
           * Stays while there is reasoning to preview, not only while it arrives. Dropping
           * it on the first reply token would shrink the row and move every row below.
           * Cut from the front, so it shows the newest words.
           */
          <Text style={styles.preview} numberOfLines={1} ellipsizeMode="head">
            {thinking.slice(-PREVIEW_TAIL_CHARS)}
          </Text>
        ) : null}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      gap: theme.spacing.xs,
    },
    header: {
      flexDirection: 'row',
      alignItems: 'center',
      alignSelf: 'flex-start',
      gap: theme.spacing.xs,
      paddingVertical: theme.spacing.xxs,
    },
    label: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.subhead,
    },
    preview: {
      color: theme.colors.tertiaryLabel,
      ...theme.typography.footnote,
    },
    body: {
      borderLeftWidth: 2,
      borderLeftColor: theme.colors.separator,
      paddingLeft: theme.spacing.md,
    },
    bodyText: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.footnote,
    },
    pressed: {
      opacity: 0.35,
    },
  })
);
