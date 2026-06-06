import { memo, useMemo } from 'react';
import { View, Text, Image, ScrollView, StyleSheet } from 'react-native';
import { AVATAR_COLORS } from './constants';
import { colors, typography, radius, ROW_INSET } from './theme';

export interface FeedElement {
  id: string;
  username: string;
  handle: string;
  text: string;
  imageUrls: string[];
  timestamp: string;
}

interface FeedElementProps {
  element: FeedElement;
  index: number;
}

export const FeedElement = memo(({ element, index }: FeedElementProps) => {
  const avatarColor = useMemo(() => {
    return AVATAR_COLORS[index % AVATAR_COLORS.length];
  }, [index]);

  return (
    <View style={styles.feedElement}>
      <View style={[styles.avatar, { backgroundColor: avatarColor }]}>
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
    paddingLeft: 16,
    paddingRight: 16,
    paddingVertical: 12,
    flexDirection: 'row',
  },
  avatar: {
    width: 40,
    height: 40,
    borderRadius: 20,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
  },
  avatarText: {
    color: colors.label,
    fontSize: 17,
    fontWeight: '600',
  },
  content: {
    flex: 1,
  },
  userInfo: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
    marginBottom: 2,
  },
  username: {
    color: colors.label,
    ...typography.subhead,
    fontWeight: '600',
  },
  handle: {
    color: colors.secondaryLabel,
    ...typography.subhead,
  },
  tweetText: {
    color: colors.label,
    ...typography.subhead,
    marginBottom: 12,
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
    paddingHorizontal: 4,
  },
  multiImageContainer: {
    width: 280,
    height: 200,
    borderRadius: radius.lg,
    overflow: 'hidden',
    backgroundColor: colors.elevated2,
    marginHorizontal: 4,
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
