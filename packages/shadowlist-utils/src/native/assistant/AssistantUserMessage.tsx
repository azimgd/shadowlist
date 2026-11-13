import { memo, useCallback, useMemo, useState } from 'react';
import {
  View,
  Text,
  Pressable,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { CopyIcon, PencilIcon } from '../icons';
import { defaultAssistantLabels, type AssistantLabels } from './labels';
import { AssistantActionButton } from './AssistantActionButton';
import { AssistantAttachmentChip } from './AssistantAttachmentChip';
import { AssistantBusy } from './rowState';
import type { AssistantPrompt } from './types';
import {
  BUBBLE_PADDING_HORIZONTAL,
  BUBBLE_PADDING_VERTICAL,
  BUBBLE_WIDTH_FRACTION,
} from './userMessageSizeSpec';

export interface AssistantUserMessageProps {
  message: AssistantPrompt;
  busy?: boolean;
  onCopy?: (text: string) => void;
  onEdit?: (messageId: string) => void;
  labels?: Partial<AssistantLabels>;
  style?: StyleProp<ViewStyle>;
}

const EMPTY_ATTACHMENTS: AssistantPrompt['attachments'] = [];

export const AssistantUserMessage = memo(
  ({
    message,
    busy = false,
    onCopy,
    onEdit,
    labels,
    style,
  }: AssistantUserMessageProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const l = useLabels(defaultAssistantLabels, labels);
    const [actionsVisible, setActionsVisible] = useState(false);
    const toggleActions = useCallback(
      () => setActionsVisible((previous) => !previous),
      []
    );
    const accessibilityActions = useMemo(
      () => [{ name: 'longpress', label: l.showActions }],
      [l.showActions]
    );
    const attachments = message.attachments ?? EMPTY_ATTACHMENTS;

    return (
      <View style={[styles.container, style]}>
        {attachments.length > 0 ? (
          <View style={styles.attachments}>
            {attachments.map((attachment) => (
              <AssistantAttachmentChip
                key={attachment.id}
                attachment={attachment}
                labels={l}
              />
            ))}
          </View>
        ) : null}

        {message.text ? (
          <Pressable
            onLongPress={toggleActions}
            delayLongPress={300}
            accessibilityRole="button"
            accessibilityHint={l.showActionsHint}
            /*
             * Screen readers can't long press. Offer the same toggle as a named action.
             * Otherwise Copy and Edit are out of reach.
             */
            accessibilityActions={accessibilityActions}
            onAccessibilityAction={toggleActions}
            style={styles.bubble}
          >
            <Text style={styles.text}>{message.text}</Text>
          </Pressable>
        ) : null}

        {actionsVisible ? (
          <View style={styles.actions}>
            <AssistantActionButton
              label={l.copy}
              showLabel
              onPress={() => {
                onCopy?.(message.text);
                setActionsVisible(false);
              }}
            >
              <CopyIcon
                size={14}
                color={theme.colors.label}
                strokeWidth={1.3}
              />
            </AssistantActionButton>
            <AssistantBusy busy={busy}>
              {(disabled) => (
                <AssistantActionButton
                  label={l.edit}
                  showLabel
                  disabled={disabled}
                  onPress={() => {
                    onEdit?.(message.id);
                    setActionsVisible(false);
                  }}
                >
                  <PencilIcon
                    size={14}
                    color={theme.colors.label}
                    strokeWidth={1.3}
                  />
                </AssistantActionButton>
              )}
            </AssistantBusy>
          </View>
        ) : null}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      paddingHorizontal: theme.spacing.lg,
      paddingVertical: theme.spacing.sm,
      alignItems: 'flex-end',
    },
    attachments: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      justifyContent: 'flex-end',
      gap: theme.spacing.sm,
      marginBottom: theme.spacing.sm,
    },
    bubble: {
      maxWidth: `${BUBBLE_WIDTH_FRACTION * 100}%`,
      paddingHorizontal: BUBBLE_PADDING_HORIZONTAL,
      paddingVertical: BUBBLE_PADDING_VERTICAL,
      borderRadius: theme.radius.xl,
      borderBottomRightRadius: 6,
      backgroundColor: theme.colors.elevated2,
    },
    text: {
      color: theme.colors.label,
      ...theme.typography.body,
    },
    actions: {
      flexDirection: 'row',
      gap: theme.spacing.sm,
      marginTop: theme.spacing.sm,
    },
  })
);
