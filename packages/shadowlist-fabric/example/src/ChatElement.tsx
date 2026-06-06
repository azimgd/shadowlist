import { memo, useMemo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';
import { AVATAR_COLORS } from './constants';
import { colors, typography, radius } from './theme';

interface ChatElementProps {
  id: string;
  index: number;
  text: string;
  isFromMe: boolean;
  imageUrl?: string;
  imageUrls?: string[];
}

export const ChatElement = memo(
  ({ index, text, isFromMe, imageUrl, imageUrls }: ChatElementProps) => {
    const avatarColor = useMemo(() => {
      return AVATAR_COLORS[index % AVATAR_COLORS.length];
    }, [index]);

    const initials = useMemo(() => {
      const firstLetter = String.fromCharCode(65 + (index % 26));
      const secondLetter = String.fromCharCode(65 + ((index * 3) % 26));
      return `${firstLetter}${secondLetter}`;
    }, [index]);

    const username = useMemo(() => {
      const names = [
        'Alice',
        'Bob',
        'Charlie',
        'Diana',
        'Eve',
        'Frank',
        'Grace',
        'Henry',
      ];
      return names[index % names.length];
    }, [index]);

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
    width: 30,
    height: 30,
    borderRadius: 15,
    marginRight: 8,
    marginBottom: 2,
    justifyContent: 'center',
    alignItems: 'center',
  },
  avatarText: {
    color: colors.label,
    fontSize: 12,
    fontWeight: '600',
  },
  bubbleColumn: {
    maxWidth: '75%',
  },
  sender: {
    color: colors.secondaryLabel,
    ...typography.caption,
    marginLeft: 12,
    marginBottom: 2,
  },
  bubble: {
    paddingHorizontal: 14,
    paddingVertical: 8,
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
