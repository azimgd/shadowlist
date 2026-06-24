import { memo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';
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
    paddingHorizontal: 14,
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
