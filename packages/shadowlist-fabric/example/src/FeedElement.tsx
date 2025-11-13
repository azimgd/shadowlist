import { memo, useMemo } from 'react';
import { View, Text, Image, ScrollView, StyleSheet } from 'react-native';
import { AVATAR_COLORS } from './constants';

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
          <Text style={styles.handle}>{element.handle} · {index}</Text>
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
    </View>
  );
});

const styles = StyleSheet.create({
  feedElement: {
    backgroundColor: '#000000',
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#2F3336',
    paddingHorizontal: 12,
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
  imageScrollView: {
    marginHorizontal: -4,
  },
  imageScrollContent: {
    paddingHorizontal: 4,
  },
  multiImageContainer: {
    width: 280,
    height: 200,
    borderRadius: 16,
    overflow: 'hidden',
    backgroundColor: '#2F3336',
    marginHorizontal: 4,
  },
  multiImage: {
    width: '100%',
    height: '115%',
    marginTop: 0,
  },
});
