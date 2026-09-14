import { memo, useState } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import Animated from 'react-native-reanimated';
import { colors, typography, spacing } from '../theme';
import { Chevron } from '../icons';
import { usePulseStyle } from './AssistantTypingIndicator';

export interface AssistantThinkingProps {
  thinking: string;
  thinkingMs: number;
  // Still reasoning: the label breathes and the collapsed row previews the latest words.
  active: boolean;
}

/*
 * Characters of reasoning kept for the collapsed one-line preview. The line is truncated to
 * one line anyway, but layout measures the WHOLE string first, so handing it a chain of
 * thought thousands of characters long costs a full text measure per flush to render ~40
 * visible characters. The tail is what the preview shows, so the tail is all it gets.
 */
const PREVIEW_TAIL_CHARS = 160;

/*
 * The breathing label, split out so the repeating animation exists only while reasoning is
 * active. A finished reply keeps its label static instead of animating forever.
 */
const PulsingLabel = ({ label }: { label: string }) => {
  const pulse = usePulseStyle();
  return <Animated.Text style={[styles.label, pulse]}>{label}</Animated.Text>;
};

/*
 * Collapsible reasoning. Collapsed by default so a long chain of thought never pushes the
 * answer off screen; while active, one line shows the tail of what is being thought.
 */
export const AssistantThinking = memo(
  ({ thinking, thinkingMs, active }: AssistantThinkingProps) => {
    const [expanded, setExpanded] = useState(false);
    const seconds = Math.max(1, Math.round(thinkingMs / 1000));
    const label = active ? 'Thinking' : `Thought for ${seconds}s`;

    return (
      <View style={styles.container}>
        <Pressable
          onPress={() => setExpanded((current) => !current)}
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
          <Chevron
            direction={expanded ? 'down' : 'right'}
            size={14}
            color={colors.secondaryLabel}
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
           * Mounted for as long as there is reasoning to preview, not only while it is
           * still arriving: dropping the line the moment the first content token lands
           * shrinks the row by a line mid-stream, which moves every row under it.
           * Head-ellipsized, so it always shows the newest words, not the first ones.
           */
          <Text style={styles.preview} numberOfLines={1} ellipsizeMode="head">
            {thinking.slice(-PREVIEW_TAIL_CHARS)}
          </Text>
        ) : null}
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    gap: spacing.xs,
  },
  header: {
    flexDirection: 'row',
    alignItems: 'center',
    alignSelf: 'flex-start',
    gap: spacing.xs,
    paddingVertical: spacing.xxs,
  },
  label: {
    color: colors.secondaryLabel,
    ...typography.subhead,
  },
  preview: {
    color: colors.tertiaryLabel,
    ...typography.footnote,
  },
  body: {
    borderLeftWidth: 2,
    borderLeftColor: colors.separator,
    paddingLeft: spacing.md,
  },
  bodyText: {
    color: colors.secondaryLabel,
    ...typography.footnote,
  },
  pressed: {
    opacity: 0.35,
  },
});
