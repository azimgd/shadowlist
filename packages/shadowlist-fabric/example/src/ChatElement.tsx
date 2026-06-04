import { memo, useMemo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';
import { AVATAR_COLORS } from './constants';

interface ChatElementProps {
  id: string;
  index: number;
  text: string;
  isFromMe: boolean;
  imageUrl?: string;
  imageUrls?: string[];
}

export const ChatElement = memo(({ id, index, text, isFromMe, imageUrl, imageUrls }: ChatElementProps) => {
  const avatarColor = useMemo(() => {
    return AVATAR_COLORS[index % AVATAR_COLORS.length];
  }, [index]);

  const initials = useMemo(() => {
    const firstLetter = String.fromCharCode(65 + (index % 26));
    const secondLetter = String.fromCharCode(65 + ((index * 3) % 26));
    return `${firstLetter}${secondLetter}`;
  }, [index]);

  const username = useMemo(() => {
    const names = ['Alice', 'Bob', 'Charlie', 'Diana', 'Eve', 'Frank', 'Grace', 'Henry'];
    return names[index % names.length];
  }, [index]);

  const hasImageGrid = imageUrls && imageUrls.length > 0;
  const hasSingleImage = imageUrl && !text;

  if (hasImageGrid) {
    return (
      <View style={[styles.container, isFromMe ? styles.containerFromMe : styles.containerFromThem]}>
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
      <View style={[styles.container, isFromMe ? styles.containerFromMe : styles.containerFromThem]}>
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
    <View style={[styles.container, isFromMe ? styles.containerFromMe : styles.containerFromThem]}>
      {!isFromMe && (
        <View style={[styles.avatar, { backgroundColor: avatarColor }]}>
          <Text style={styles.avatarText}>{initials}</Text>
        </View>
      )}
      <View style={[styles.bubble, isFromMe ? styles.bubbleFromMe : styles.bubbleFromThem]}>
        <Text style={styles.username}>
          {username} · {index}
        </Text>
        <Text style={[styles.text, isFromMe ? styles.textFromMe : styles.textFromThem]}>
          {text}
        </Text>
        <Text style={styles.timestamp}>
          {new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}
        </Text>
      </View>
    </View>
  );
});

const styles = StyleSheet.create({
  container: {
    paddingHorizontal: 12,
    paddingVertical: 2,
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
    width: 32,
    height: 32,
    borderRadius: 16,
    marginRight: 8,
    marginBottom: 2,
    justifyContent: 'center',
    alignItems: 'center',
  },
  avatarText: {
    color: '#FFFFFF',
    fontSize: 12,
    fontWeight: '600',
  },
  bubble: {
    maxWidth: '75%',
    paddingHorizontal: 16,
    paddingVertical: 10,
    borderRadius: 20,
    marginVertical: 2,
  },
  bubbleFromMe: {
    backgroundColor: '#0A84FF',
    borderBottomRightRadius: 4,
  },
  bubbleFromThem: {
    backgroundColor: '#2C2C2E',
    borderBottomLeftRadius: 4,
  },
  username: {
    color: '#FFFFFF',
    fontSize: 14,
    fontWeight: '600',
    marginBottom: 4,
    opacity: 0.8,
  },
  imageUsername: {
    color: '#FFFFFF',
    fontSize: 14,
    fontWeight: '600',
    marginBottom: 4,
    opacity: 0.8,
  },
  text: {
    fontSize: 16,
    lineHeight: 20,
  },
  textFromMe: {
    color: '#FFFFFF',
  },
  textFromThem: {
    color: '#FFFFFF',
  },
  timestamp: {
    fontSize: 12,
    marginTop: 4,
    opacity: 0.5,
    color: '#FFFFFF',
  },
  singleImageContainer: {
    width: 240,
    height: 320,
    borderRadius: 16,
    overflow: 'hidden',
    backgroundColor: '#2F3336',
    marginVertical: 2,
  },
  singleImage: {
    width: '100%',
    height: '115%',
    marginTop: 0,
  },
  imageGridContainer: {
    width: 240,
    marginVertical: 2,
  },
  imageGridRow: {
    flexDirection: 'row',
    gap: 2,
    marginBottom: 2,
  },
  imageGridElement: {
    width: 119,
    height: 119,
    borderRadius: 8,
    overflow: 'hidden',
    backgroundColor: '#2F3336',
  },
  imageGridImage: {
    width: '100%',
    height: '115%',
    marginTop: 0,
  },
});
