import { memo, useCallback, useState } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import type { ElementSizeSpec } from 'shadowlist';
import { colors, typography, spacing, radius } from '../theme';
import { Copy, Pencil } from '../icons';
import { AssistantActionButton } from './AssistantActionButton';
import { AssistantAttachmentChip } from './AssistantAttachmentChip';
import type { AssistantPrompt } from './data';

export interface AssistantUserMessageProps {
  message: AssistantPrompt;
  onCopy?: (text: string) => void;
  // Called with the message id; the screen loads it into the composer for resending.
  onEdit?: (messageId: string) => void;
  /*
   * A reply is streaming. Editing rewinds the conversation from this prompt on, which would
   * strand the in-flight reply, so it waits until the stream finishes.
   */
  busy?: boolean;
}

/*
 * What a plain-text user message measures to, derived from the styles below. Native
 * computes the real height from it before the row renders, so sending a message never
 * lands on an estimated height and reflows the reply under it.
 *
 * The numbers are a transcription of the stylesheet and have to stay one: a spec that
 * disagrees with its row predicts a confidently wrong height.
 *
 *   container  paddingHorizontal 16, paddingVertical 8
 *   bubble     maxWidth 80% of the container's content box, padding 14 / 10
 *   text       typography.body
 */
const BUBBLE_WIDTH_FRACTION = 0.8;
// 0.8 x (W - 32) - 28  ==  0.8 x W - 53.6
const BUBBLE_INSET_WIDTH = 53.6;
// container 8+8, bubble 10+10
const BUBBLE_INSET_HEIGHT = 36;

// The long-press toggle, exposed to assistive technology as a named action.
const ACTIONS_ACCESSIBILITY_ACTIONS = [
  { name: 'longpress', label: 'Show actions' },
];

// Attachments add rows the spec can't describe; those messages are measured natively.
export function getUserMessageSizeSpec(
  message: AssistantPrompt
): ElementSizeSpec | null {
  if (!message.text || message.attachments.length > 0) return null;

  return {
    text: message.text,
    fontSize: typography.body.fontSize,
    lineHeight: typography.body.lineHeight,
    letterSpacing: typography.body.letterSpacing,
    widthFraction: BUBBLE_WIDTH_FRACTION,
    insetWidth: BUBBLE_INSET_WIDTH,
    insetHeight: BUBBLE_INSET_HEIGHT,
  };
}

/*
 * Right-aligned prompt bubble with its attachments above it. Long-press reveals Copy and
 * Edit; the revealed row is measured natively, since a real measurement always outranks
 * the collapsed prediction.
 */
export const AssistantUserMessage = memo(
  ({ message, onCopy, onEdit, busy = false }: AssistantUserMessageProps) => {
    const [actionsVisible, setActionsVisible] = useState(false);
    const toggleActions = useCallback(
      () => setActionsVisible((current) => !current),
      []
    );

    return (
      <View style={styles.container}>
        {message.attachments.length > 0 ? (
          <View style={styles.attachments}>
            {message.attachments.map((attachment) => (
              <AssistantAttachmentChip
                key={attachment.id}
                attachment={attachment}
              />
            ))}
          </View>
        ) : null}

        {message.text ? (
          <Pressable
            onLongPress={toggleActions}
            delayLongPress={300}
            accessibilityRole="button"
            accessibilityHint="Long press for copy and edit"
            /*
             * A long press is not reachable by assistive technology, so the same toggle is
             * exposed as a named custom action. Without it Copy and Edit simply do not
             * exist for a screen reader.
             */
            accessibilityActions={ACTIONS_ACCESSIBILITY_ACTIONS}
            onAccessibilityAction={toggleActions}
            style={styles.bubble}
          >
            <Text style={styles.text}>{message.text}</Text>
          </Pressable>
        ) : null}

        {actionsVisible ? (
          <View style={styles.actions}>
            <AssistantActionButton
              label="Copy"
              showLabel
              onPress={() => {
                onCopy?.(message.text);
                setActionsVisible(false);
              }}
            >
              <Copy size={14} color={colors.label} strokeWidth={1.3} />
            </AssistantActionButton>
            <AssistantActionButton
              label="Edit"
              showLabel
              disabled={busy}
              onPress={() => {
                onEdit?.(message.id);
                setActionsVisible(false);
              }}
            >
              <Pencil size={14} color={colors.label} strokeWidth={1.3} />
            </AssistantActionButton>
          </View>
        ) : null}
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    paddingHorizontal: spacing.lg,
    paddingVertical: spacing.sm,
    alignItems: 'flex-end',
  },
  attachments: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'flex-end',
    gap: spacing.sm,
    marginBottom: spacing.sm,
  },
  bubble: {
    maxWidth: '80%',
    paddingHorizontal: 14,
    paddingVertical: 10,
    borderRadius: radius.xl,
    borderBottomRightRadius: 6,
    backgroundColor: colors.elevated2,
  },
  text: {
    color: colors.label,
    ...typography.body,
  },
  actions: {
    flexDirection: 'row',
    gap: spacing.sm,
    marginTop: spacing.sm,
  },
});
