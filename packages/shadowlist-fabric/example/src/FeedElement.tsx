import { memo, useMemo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';

export interface FeedElement {
  id: string;
  username: string;
  handle: string;
  text: string;
  imageUrl: string;
  timestamp: string;
}

const AVATAR_COLORS = [
  '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A', '#98D8C8',
  '#F7DC6F', '#BB8FCE', '#85C1E2', '#F8B195', '#C06C84',
];

interface FeedElementProps {
  element: FeedElement;
  index: number;
}

export const FeedElement = memo(({ element, index }: FeedElementProps) => {
  const avatarColor = useMemo(() => {
    return AVATAR_COLORS[index % AVATAR_COLORS.length];
  }, [index]);

  return (
    <View style={styles.feedItem}>
      <View style={[styles.avatar, { backgroundColor: avatarColor }]}>
        <Text style={styles.avatarText}>{element.username.charAt(0)}</Text>
      </View>
      <View style={styles.content}>
        <View style={styles.userInfo}>
          <Text style={styles.username}>{element.username}</Text>
          <Text style={styles.handle}>{element.handle}</Text>
          <Text style={styles.timestamp}>· {element.timestamp}</Text>
        </View>
        <Text style={styles.tweetText}>{element.text}</Text>
        <View style={styles.imageContainer}>
          <Image
            source={{ uri: element.imageUrl }}
            style={styles.image}
            resizeMode="cover"
          />
        </View>
      </View>
    </View>
  );
});

const styles = StyleSheet.create({
  feedItem: {
    backgroundColor: '#000000',
    borderBottomWidth: 0.5,
    borderBottomColor: '#2F3336',
    paddingHorizontal: 16,
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
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: '700',
  },
  content: {
    flex: 1,
  },
  userInfo: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
    marginBottom: 4,
  },
  username: {
    color: '#FFFFFF',
    fontSize: 15,
    fontWeight: '700',
  },
  handle: {
    color: '#71767B',
    fontSize: 15,
  },
  timestamp: {
    color: '#71767B',
    fontSize: 15,
  },
  tweetText: {
    color: '#FFFFFF',
    fontSize: 15,
    lineHeight: 20,
    marginBottom: 12,
  },
  imageContainer: {
    width: '100%',
    height: 200,
    borderRadius: 16,
    overflow: 'hidden',
    backgroundColor: '#2F3336',
  },
  image: {
    width: '100%',
    height: '115%',
    marginTop: 0,
  },
});
