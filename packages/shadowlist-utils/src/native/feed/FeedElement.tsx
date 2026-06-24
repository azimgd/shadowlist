import { memo } from 'react';
import { View, Text, Image, ScrollView, StyleSheet } from 'react-native';
import { type FeedItem } from 'shadowlist-utils';
import {
  colors,
  typography,
  radius,
  ROW_INSET,
  spacing,
  fontSize,
  fontWeight,
} from '../theme';

export interface FeedElementProps {
  element: FeedItem;
}

export const FeedElement = memo(({ element }: FeedElementProps) => {
  return (
    <View style={styles.feedElement}>
      <View style={[styles.avatar, { backgroundColor: element.avatarColor }]}>
        <Text style={styles.avatarText}>{element.username.charAt(0)}</Text>
      </View>
      <View style={styles.content}>
        <View style={styles.userInfo}>
          <Text style={styles.username}>{element.username}</Text>
          <Text style={styles.handle}>{element.handle}</Text>
        </View>
        <Text style={styles.tweetText}>{element.text}</Text>
        {element.imageUrls.length === 1 ? (
          <View style={styles.imageContainer}>
            <Image
              source={{ uri: element.imageUrls[0] }}
              style={styles.image}
              resizeMode="cover"
            />
          </View>
        ) : (
          <ScrollView
            horizontal
            showsHorizontalScrollIndicator={false}
            style={styles.imageScrollView}
            contentContainerStyle={styles.imageScrollContent}
          >
            {element.imageUrls.map((imageUrl, imageIndex) => (
              <View key={imageIndex} style={styles.multiImageContainer}>
                <Image
                  source={{ uri: imageUrl }}
                  style={styles.multiImage}
                  resizeMode="cover"
                />
              </View>
            ))}
          </ScrollView>
        )}
      </View>
      <View style={styles.separator} />
    </View>
  );
});

const styles = StyleSheet.create({
  feedElement: {
    backgroundColor: colors.background,
    paddingLeft: spacing.lg,
    paddingRight: spacing.lg,
    paddingVertical: spacing.md,
    flexDirection: 'row',
  },
  avatar: {
    width: 40,
    height: 40,
    borderRadius: radius.xl,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: spacing.md,
  },
  avatarText: {
    color: colors.label,
    fontSize: fontSize.body,
    fontWeight: fontWeight.semibold,
  },
  content: {
    flex: 1,
  },
  userInfo: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.xs,
    marginBottom: spacing.xxs,
  },
  username: {
    color: colors.label,
    ...typography.subhead,
    fontWeight: fontWeight.semibold,
  },
  handle: {
    color: colors.secondaryLabel,
    ...typography.subhead,
  },
  tweetText: {
    color: colors.label,
    ...typography.subhead,
    marginBottom: spacing.md,
  },
  imageContainer: {
    width: '100%',
    height: 200,
    borderRadius: radius.lg,
    overflow: 'hidden',
    backgroundColor: colors.elevated2,
  },
  image: {
    width: '100%',
    height: '115%',
    marginTop: 0,
  },
  imageScrollView: {
    marginHorizontal: -4,
  },
  imageScrollContent: {
    paddingHorizontal: spacing.xs,
  },
  multiImageContainer: {
    width: 280,
    height: 200,
    borderRadius: radius.lg,
    overflow: 'hidden',
    backgroundColor: colors.elevated2,
    marginHorizontal: spacing.xs,
  },
  multiImage: {
    width: '100%',
    height: '115%',
    marginTop: 0,
  },
  separator: {
    position: 'absolute',
    left: ROW_INSET,
    right: 0,
    bottom: 0,
    height: StyleSheet.hairlineWidth,
    backgroundColor: colors.separator,
  },
});
