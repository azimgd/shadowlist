import { memo } from 'react';
import {
  Image,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type TextStyle,
  type ViewStyle,
} from 'react-native';
import { formatRelativeTime } from '../formatRelativeTime';
import { useLabels } from '../labels';
import { Avatar } from '../primitives/Avatar';
import { useLargeText } from '../internal/useLargeText';
import { createStyles } from '../theme';
import { defaultFeedLabels, type FeedLabels } from './labels';
import type { FeedImage, FeedItem } from './types';

export interface FeedRowProps {
  item: FeedItem;
  onPress?: (item: FeedItem) => void;
  onPressImage?: (item: FeedItem, imageIndex: number) => void;
  formatTime?: (createdAt: Date | number) => string;
  labels?: Partial<FeedLabels>;
  style?: StyleProp<ViewStyle>;
  avatarStyle?: StyleProp<ViewStyle>;
  textStyle?: StyleProp<TextStyle>;
}

const SINGLE_IMAGE_HEIGHT = 200;
const MIN_ASPECT_RATIO = 3 / 4;
const MAX_ASPECT_RATIO = 16 / 9;

function getSingleImageFrame(image: FeedImage): ViewStyle {
  if (image.width && image.height) {
    const ratio = image.width / image.height;
    return {
      aspectRatio: Math.min(
        MAX_ASPECT_RATIO,
        Math.max(MIN_ASPECT_RATIO, ratio)
      ),
    };
  }
  return { height: SINGLE_IMAGE_HEIGHT };
}

export const FeedRow = memo(
  ({
    item,
    onPress,
    onPressImage,
    formatTime,
    labels,
    style,
    avatarStyle,
    textStyle,
  }: FeedRowProps) => {
    const styles = useStyles();
    const largeText = useLargeText();
    const lines = largeText ? undefined : 1;
    const l = useLabels(defaultFeedLabels, labels);
    const { author, text, images, createdAt } = item;
    const hasImages = images !== undefined && images.length > 0;
    const date =
      createdAt === undefined
        ? undefined
        : (formatTime?.(createdAt) ?? formatRelativeTime(createdAt, l));

    const renderImage = (
      image: FeedImage,
      index: number,
      frameStyle: StyleProp<ViewStyle>
    ) => {
      const content = (
        <Image
          source={{ uri: image.uri }}
          style={styles.image}
          resizeMode="cover"
        />
      );
      if (onPressImage === undefined) {
        return (
          <View
            key={index}
            style={frameStyle}
            accessible={image.alt !== undefined}
            accessibilityRole="image"
            accessibilityLabel={image.alt}
          >
            {content}
          </View>
        );
      }
      return (
        <Pressable
          key={index}
          style={frameStyle}
          accessibilityRole="imagebutton"
          accessibilityLabel={image.alt ?? l.image}
          onPress={() => onPressImage(item, index)}
        >
          {content}
        </Pressable>
      );
    };

    const body = (
      <>
        <Avatar
          name={author.name}
          uri={author.avatarUrl}
          color={author.avatarColor}
          style={[styles.avatar, avatarStyle]}
        />
        <View style={styles.content}>
          <View style={[styles.header, largeText && styles.headerWrapped]}>
            <Text style={styles.name} numberOfLines={lines}>
              {author.name}
            </Text>
            {author.handle !== undefined && (
              <Text style={styles.secondary} numberOfLines={lines}>
                {author.handle}
              </Text>
            )}
            {date !== undefined && (
              <Text style={styles.date} numberOfLines={lines}>
                · {date}
              </Text>
            )}
          </View>
          {text !== undefined && text.length > 0 && (
            <Text
              style={[
                styles.text,
                hasImages && styles.textAboveImages,
                textStyle,
              ]}
            >
              {text}
            </Text>
          )}
          {hasImages &&
            images.length === 1 &&
            renderImage(images[0]!, 0, [
              styles.imageFrame,
              getSingleImageFrame(images[0]!),
            ])}
          {hasImages && images.length > 1 && (
            <ScrollView
              horizontal
              showsHorizontalScrollIndicator={false}
              style={styles.strip}
              contentContainerStyle={styles.stripContent}
            >
              {images.map((image, index) =>
                renderImage(image, index, [
                  styles.imageFrame,
                  styles.stripImage,
                ])
              )}
            </ScrollView>
          )}
        </View>
        <View style={styles.separator} />
      </>
    );

    if (onPress === undefined) {
      return <View style={[styles.row, style]}>{body}</View>;
    }
    return (
      <Pressable
        style={[styles.row, style]}
        accessibilityRole="button"
        onPress={() => onPress(item)}
      >
        {body}
      </Pressable>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    row: {
      backgroundColor: theme.colors.background,
      paddingHorizontal: theme.spacing.lg,
      paddingVertical: theme.spacing.md,
      flexDirection: 'row',
    },
    avatar: {
      marginRight: theme.spacing.md,
    },
    content: {
      flex: 1,
    },
    header: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: theme.spacing.xs,
      marginBottom: theme.spacing.xxs,
    },
    headerWrapped: {
      flexWrap: 'wrap',
    },
    name: {
      color: theme.colors.label,
      ...theme.typography.subhead,
      fontWeight: theme.fontWeight.semibold,
      flexShrink: 1,
    },
    secondary: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.subhead,
      flexShrink: 1,
    },
    date: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.subhead,
    },
    text: {
      color: theme.colors.label,
      ...theme.typography.subhead,
    },
    textAboveImages: {
      marginBottom: theme.spacing.md,
    },
    imageFrame: {
      borderRadius: theme.radius.lg,
      overflow: 'hidden',
      backgroundColor: theme.colors.elevated2,
    },
    image: {
      width: '100%',
      height: '100%',
    },
    strip: {
      marginHorizontal: -theme.spacing.xs,
    },
    stripContent: {
      paddingHorizontal: theme.spacing.xs,
    },
    stripImage: {
      width: 280,
      height: SINGLE_IMAGE_HEIGHT,
      marginHorizontal: theme.spacing.xs,
    },
    separator: {
      position: 'absolute',
      left: theme.rowInset,
      right: 0,
      bottom: 0,
      height: StyleSheet.hairlineWidth,
      backgroundColor: theme.colors.separator,
    },
  })
);
