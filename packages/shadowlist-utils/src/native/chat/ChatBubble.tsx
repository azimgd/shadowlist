import { memo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';
import type { ElementSizeSpec } from 'shadowlist';
import {
  colors,
  typography,
  radius,
  spacing,
  fontSize,
  fontWeight,
} from '../theme';

export interface ChatMessage {
  id: string;
  text: string;
  isFromMe: boolean;
  imageUrl?: string;
  imageUrls?: string[];
  username?: string;
  avatarColor?: string;
  initials?: string;
}

export interface ChatBubbleProps {
  text?: string;
  isFromMe?: boolean;
  imageUrl?: string;
  imageUrls?: string[];
  username?: string;
  avatarColor?: string;
  initials?: string;
}

/*
 * What a text bubble measures to, derived from the styles below. Native computes the real
 * height from it before the row renders, so the list has correct geometry on the first
 * frame instead of estimating every bubble and reflowing the ones below it -- which on an
 * inverted chat list also means the scroll position stops being nudged while you read.
 *
 * The insets are a transcription of the stylesheet and have to stay one: a spec that
 * disagrees with its row predicts a confidently wrong height.
 *
 *   container     paddingHorizontal spacing.md, paddingVertical spacing.xxs
 *   avatar        beside the column, not inside it, so it narrows nothing
 *   bubbleColumn  maxWidth 75% of the container's content box
 *   sender        typography.caption line + spacing.xxs margin, on messages from others
 *   bubble        paddingHorizontal 14, paddingVertical spacing.sm
 *   text          typography.body
 */
const BUBBLE_WIDTH_FRACTION = 0.75;
const BUBBLE_PADDING_HORIZONTAL = 14;
// 0.75 x (W - 2 x container padding) - 2 x bubble padding
const BUBBLE_INSET_WIDTH =
  BUBBLE_WIDTH_FRACTION * 2 * spacing.md + 2 * BUBBLE_PADDING_HORIZONTAL;
const BUBBLE_INSET_HEIGHT = 2 * spacing.xxs + 2 * spacing.sm;
const SENDER_HEIGHT = typography.caption.lineHeight + spacing.xxs;

// Image rows get their height from the images, not from text, so they are measured natively.
export function getChatMessageSizeSpec(
  message: ChatMessage
): ElementSizeSpec | null {
  if (!message.text) return null;

  return {
    text: message.text,
    fontSize: typography.body.fontSize,
    lineHeight: typography.body.lineHeight,
    letterSpacing: typography.body.letterSpacing,
    widthFraction: BUBBLE_WIDTH_FRACTION,
    insetWidth: BUBBLE_INSET_WIDTH,
    insetHeight: BUBBLE_INSET_HEIGHT + (message.isFromMe ? 0 : SENDER_HEIGHT),
  };
}

export const ChatBubble = memo(
  ({
    text = '',
    isFromMe = false,
    imageUrl,
    imageUrls,
    username = '',
    avatarColor,
    initials = '',
  }: ChatBubbleProps) => {
    const hasImageGrid = imageUrls && imageUrls.length > 0;
    const hasSingleImage = imageUrl && !text;

    if (hasImageGrid) {
      return (
        <View
          style={[
            styles.container,
            isFromMe ? styles.containerFromMe : styles.containerFromThem,
          ]}
        >
          {!isFromMe && (
            <View style={[styles.avatar, { backgroundColor: avatarColor }]}>
              <Text style={styles.avatarText}>{initials}</Text>
            </View>
          )}
          <View style={styles.imageGridContainer}>
            <View style={styles.imageGridRow}>
              <View style={styles.imageGridElement}>
                <Image
                  source={{ uri: imageUrls[0] }}
                  style={styles.imageGridImage}
                  resizeMode="cover"
                />
              </View>
              <View style={styles.imageGridElement}>
                <Image
                  source={{ uri: imageUrls[1] }}
                  style={styles.imageGridImage}
                  resizeMode="cover"
                />
              </View>
            </View>
            <View style={styles.imageGridRow}>
              <View style={styles.imageGridElement}>
                <Image
                  source={{ uri: imageUrls[2] }}
                  style={styles.imageGridImage}
                  resizeMode="cover"
                />
              </View>
              <View style={styles.imageGridElement}>
                <Image
                  source={{ uri: imageUrls[3] }}
                  style={styles.imageGridImage}
                  resizeMode="cover"
                />
              </View>
            </View>
          </View>
        </View>
      );
    }

    if (hasSingleImage) {
      return (
        <View
          style={[
            styles.container,
            isFromMe ? styles.containerFromMe : styles.containerFromThem,
          ]}
        >
          {!isFromMe && (
            <View style={[styles.avatar, { backgroundColor: avatarColor }]}>
              <Text style={styles.avatarText}>{initials}</Text>
            </View>
          )}
          <View style={styles.singleImageContainer}>
            <Image
              source={{ uri: imageUrl }}
              style={styles.singleImage}
              resizeMode="cover"
            />
          </View>
        </View>
      );
    }

    return (
      <View
        style={[
          styles.container,
          isFromMe ? styles.containerFromMe : styles.containerFromThem,
        ]}
      >
        {!isFromMe && (
          <View style={[styles.avatar, { backgroundColor: avatarColor }]}>
            <Text style={styles.avatarText}>{initials}</Text>
          </View>
        )}
        <View style={styles.bubbleColumn}>
          {!isFromMe && <Text style={styles.sender}>{username}</Text>}
          <View
            style={[
              styles.bubble,
              isFromMe ? styles.bubbleFromMe : styles.bubbleFromThem,
            ]}
          >
            <Text style={styles.text}>{text}</Text>
          </View>
        </View>
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    paddingHorizontal: spacing.md,
    paddingVertical: spacing.xxs,
    flexDirection: 'row',
    alignItems: 'flex-end',
  },
  containerFromMe: {
    justifyContent: 'flex-end',
  },
  containerFromThem: {
    justifyContent: 'flex-start',
  },
  avatar: {
    width: 30,
    height: 30,
    borderRadius: 15,
    marginRight: spacing.sm,
    marginBottom: spacing.xxs,
    justifyContent: 'center',
    alignItems: 'center',
  },
  avatarText: {
    color: colors.label,
    fontSize: fontSize.caption,
    fontWeight: fontWeight.semibold,
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
  bubbleFromMe: {
    backgroundColor: colors.blue,
    borderBottomRightRadius: 5,
    alignSelf: 'flex-end',
  },
  bubbleFromThem: {
    backgroundColor: colors.elevated2,
    borderBottomLeftRadius: 5,
    alignSelf: 'flex-start',
  },
  text: {
    color: colors.label,
    ...typography.body,
  },
  singleImageContainer: {
    width: 240,
    height: 320,
    borderRadius: radius.lg,
    overflow: 'hidden',
    backgroundColor: colors.elevated2,
    marginVertical: spacing.xxs,
  },
  singleImage: {
    width: '100%',
    height: '115%',
    marginTop: 0,
  },
  imageGridContainer: {
    width: 240,
    marginVertical: spacing.xxs,
  },
  imageGridRow: {
    flexDirection: 'row',
    gap: spacing.xxs,
    marginBottom: spacing.xxs,
  },
  imageGridElement: {
    width: 119,
    height: 119,
    borderRadius: radius.sm,
    overflow: 'hidden',
    backgroundColor: colors.elevated2,
  },
  imageGridImage: {
    width: '100%',
    height: '115%',
    marginTop: 0,
  },
});
