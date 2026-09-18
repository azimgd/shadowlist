import { memo } from 'react';
import {
  View,
  Text,
  Image,
  Pressable,
  StyleSheet,
  type StyleProp,
  type TextStyle,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { Avatar } from '../primitives/Avatar';
import { createStyles } from '../theme';
import { defaultChatLabels, type ChatLabels } from './labels';
import { BUBBLE_PADDING_HORIZONTAL } from './sizeSpec';
import type { ChatMessage } from './types';

export interface ChatBubbleProps {
  message: ChatMessage;
  // A line under the message. Pass `{ caption: true }` to getChatMessageSizeSpec when set.
  caption?: string;
  // Makes a failed message's status line a retry button. Keep it stable: rows are memoized.
  onRetry?: (message: ChatMessage) => void;
  onLongPress?: (message: ChatMessage) => void;
  labels?: Partial<ChatLabels>;
  style?: StyleProp<ViewStyle>;
  bubbleStyle?: StyleProp<ViewStyle>;
  textStyle?: StyleProp<TextStyle>;
}

const GRID_LIMIT = 4;
const AVATAR_SIZE = 30;

export const ChatBubble = memo(
  ({
    message,
    caption,
    onRetry,
    onLongPress,
    labels,
    style,
    bubbleStyle,
    textStyle,
  }: ChatBubbleProps) => {
    const styles = useStyles();
    const l = useLabels(defaultChatLabels, labels);
    const { isOwn, text, images = [], status } = message;
    const sending = status === 'sending';

    // A plain View unless there is a long-press: Pressable costs more per mounted row.
    const BubbleView = onLongPress ? Pressable : View;
    const bubble = text ? (
      <BubbleView
        onLongPress={onLongPress ? () => onLongPress(message) : undefined}
        accessibilityState={sending ? { busy: true } : undefined}
        accessibilityHint={sending ? l.sending : undefined}
        style={[
          styles.bubble,
          isOwn ? styles.bubbleOwn : styles.bubbleOther,
          sending && styles.sending,
          bubbleStyle,
        ]}
      >
        <Text style={[styles.text, isOwn && styles.textOwn, textStyle]}>
          {text}
        </Text>
        {caption ? (
          <Text
            style={[styles.captionInline, isOwn && styles.captionInlineOwn]}
          >
            {caption}
          </Text>
        ) : null}
      </BubbleView>
    ) : null;

    // Its height is part of getChatMessageSizeSpec; keep the two in step.
    const failedLine =
      status === 'failed' ? (
        <Pressable
          onPress={onRetry ? () => onRetry(message) : undefined}
          disabled={!onRetry}
          hitSlop={8}
          accessibilityRole={onRetry ? 'button' : undefined}
          accessibilityHint={onRetry ? l.retryHint : undefined}
          style={styles.alignEnd}
        >
          <Text style={styles.failed}>{l.failed}</Text>
        </Pressable>
      ) : null;

    let content;
    if (images.length > 0) {
      content = (
        <View style={images.length > 1 && styles.imageGridColumn}>
          {images.length === 1 ? (
            <View style={styles.singleImageContainer}>
              <Image
                source={{ uri: images[0] }}
                style={styles.image}
                resizeMode="cover"
                accessibilityLabel={l.image}
              />
            </View>
          ) : (
            <View style={styles.imageGrid}>
              {images.slice(0, GRID_LIMIT).map((uri, index) => (
                <View key={index} style={styles.imageGridCell}>
                  <Image
                    source={{ uri }}
                    style={styles.image}
                    resizeMode="cover"
                    accessibilityLabel={l.image}
                  />
                </View>
              ))}
            </View>
          )}
          {bubble}
          {caption && !text ? (
            <Text
              style={[
                styles.captionLine,
                isOwn ? styles.alignEnd : styles.alignStart,
              ]}
            >
              {caption}
            </Text>
          ) : null}
          {failedLine}
        </View>
      );
    } else {
      content = (
        <View style={styles.bubbleColumn}>
          {!isOwn && <Text style={styles.sender}>{message.author.name}</Text>}
          {bubble}
          {failedLine}
        </View>
      );
    }

    return (
      <View
        style={[
          styles.container,
          isOwn ? styles.containerOwn : styles.containerOther,
          style,
        ]}
      >
        {!isOwn && (
          <Avatar
            name={message.author.name}
            uri={message.author.avatarUrl}
            color={message.author.avatarColor}
            size={AVATAR_SIZE}
            style={styles.avatar}
          />
        )}
        {content}
      </View>
    );
  }
);

const useStyles = createStyles(({ colors, typography, spacing, radius }) =>
  StyleSheet.create({
    container: {
      paddingHorizontal: spacing.md,
      paddingVertical: spacing.xxs,
      flexDirection: 'row',
      alignItems: 'flex-end',
    },
    containerOwn: {
      justifyContent: 'flex-end',
    },
    containerOther: {
      justifyContent: 'flex-start',
    },
    avatar: {
      marginRight: spacing.sm,
      marginBottom: spacing.xxs,
    },
    bubbleColumn: {
      maxWidth: '75%',
    },
    sender: {
      color: colors.secondaryLabel,
      ...typography.caption,
      marginLeft: spacing.md,
      marginBottom: spacing.xxs,
    },
    bubble: {
      paddingHorizontal: BUBBLE_PADDING_HORIZONTAL,
      paddingVertical: spacing.sm,
      borderRadius: radius.lg + 2,
    },
    bubbleOwn: {
      backgroundColor: colors.blue,
      borderBottomRightRadius: 5,
      alignSelf: 'flex-end',
    },
    bubbleOther: {
      backgroundColor: colors.elevated2,
      borderBottomLeftRadius: 5,
      alignSelf: 'flex-start',
    },
    text: {
      color: colors.label,
      ...typography.body,
    },
    textOwn: {
      color: colors.onAccent,
    },
    captionInline: {
      color: colors.secondaryLabel,
      ...typography.caption,
      marginTop: spacing.xs,
    },
    captionInlineOwn: {
      color: colors.onAccent,
      opacity: 0.65,
    },
    sending: {
      opacity: 0.6,
    },
    failed: {
      color: colors.red,
      ...typography.caption,
      marginTop: spacing.xs,
    },
    captionLine: {
      color: colors.secondaryLabel,
      ...typography.caption,
      marginTop: spacing.xs,
    },
    alignEnd: {
      alignSelf: 'flex-end',
    },
    alignStart: {
      alignSelf: 'flex-start',
    },
    singleImageContainer: {
      width: 240,
      height: 320,
      borderRadius: radius.lg,
      overflow: 'hidden',
      backgroundColor: colors.elevated2,
      marginVertical: spacing.xxs,
    },
    imageGrid: {
      width: 240,
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: spacing.xxs,
      marginVertical: spacing.xxs,
    },
    imageGridColumn: {
      paddingBottom: spacing.xxs,
    },
    imageGridCell: {
      width: 119,
      height: 119,
      borderRadius: radius.sm,
      overflow: 'hidden',
      backgroundColor: colors.elevated2,
    },
    image: {
      width: '100%',
      height: '115%',
    },
  })
);
