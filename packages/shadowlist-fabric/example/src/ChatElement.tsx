import { memo, useMemo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';

interface ChatElementProps {
  id: string;
  index: number;
  text: string;
  isFromMe: boolean;
  imageUrl?: string;
}

const AVATAR_COLORS = [
  '#FF6B6B', '#4ECDC4', '#45B7D1', '#FFA07A', '#98D8C8',
  '#F7DC6F', '#BB8FCE', '#85C1E2', '#F8B195', '#C06C84',
];

export const ChatElement = memo(({ id, index, text, isFromMe, imageUrl }: ChatElementProps) => {
  const avatarColor = useMemo(() => {
    return AVATAR_COLORS[index % AVATAR_COLORS.length];
  }, [index]);

  const initials = useMemo(() => {
    return `U${index % 100}`;
  }, [index]);

  return (
    <View style={[styles.container, isFromMe ? styles.containerFromMe : styles.containerFromThem]}>
      {!isFromMe && (
        <View style={[styles.avatar, { backgroundColor: avatarColor }]}>
          <Text style={styles.avatarText}>{initials}</Text>
        </View>
      )}
      <View style={[styles.bubble, isFromMe ? styles.bubbleFromMe : styles.bubbleFromThem]}>
        <Text style={[styles.text, isFromMe ? styles.textFromMe : styles.textFromThem]}>
          {index}. {text}
        </Text>
        {imageUrl && (
          <View style={styles.imageContainer}>
            <Image
              source={{ uri: imageUrl }}
              style={styles.image}
              resizeMode="cover"
            />
          </View>
        )}
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
  imageContainer: {
    width: '100%',
    height: 150,
    borderRadius: 12,
    overflow: 'hidden',
    backgroundColor: '#2F3336',
    marginTop: 8,
  },
  image: {
    width: '100%',
    height: '115%',
    marginTop: 0,
  },
});
