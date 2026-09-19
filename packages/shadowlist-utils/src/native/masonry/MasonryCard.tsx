import { memo } from 'react';
import {
  Image,
  Pressable,
  StyleSheet,
  Text,
  View,
  type ImageStyle,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { createStyles } from '../theme';
import { defaultMasonryLabels, type MasonryLabels } from './labels';
import type { MasonryItem } from './types';

export interface MasonryCardProps {
  item: MasonryItem;
  onPress?: (item: MasonryItem) => void;
  labels?: Partial<MasonryLabels>;
  style?: StyleProp<ViewStyle>;
  imageStyle?: StyleProp<ImageStyle>;
}

export const MasonryCard = memo(
  ({ item, onPress, labels, style, imageStyle }: MasonryCardProps) => {
    const styles = useStyles();
    const l = useLabels(defaultMasonryLabels, labels);
    const { image, title } = item;
    const aspectRatio = image.height > 0 ? image.width / image.height : 1;
    const accessibilityLabel = image.alt ?? title ?? l.image;

    const content = (
      <>
        <View style={[styles.imageFrame, { aspectRatio }]}>
          <Image
            source={{ uri: image.uri }}
            style={[styles.image, imageStyle]}
            resizeMode="cover"
          />
        </View>
        {title !== undefined && (
          <Text style={styles.title} numberOfLines={2}>
            {title}
          </Text>
        )}
      </>
    );

    if (onPress === undefined) {
      return (
        <View
          style={[styles.card, style]}
          accessible
          accessibilityRole="image"
          accessibilityLabel={accessibilityLabel}
        >
          {content}
        </View>
      );
    }
    return (
      <Pressable
        style={[styles.card, style]}
        accessibilityRole="imagebutton"
        accessibilityLabel={accessibilityLabel}
        onPress={() => onPress(item)}
      >
        {content}
      </Pressable>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    card: {
      backgroundColor: theme.colors.background,
      marginBottom: theme.spacing.md,
      paddingHorizontal: 6,
    },
    imageFrame: {
      width: '100%',
      borderRadius: theme.radius.md,
      overflow: 'hidden',
      backgroundColor: theme.colors.elevated2,
      marginBottom: theme.spacing.sm,
    },
    image: {
      width: '100%',
      height: '100%',
    },
    title: {
      color: theme.colors.label,
      ...theme.typography.subhead,
      paddingHorizontal: theme.spacing.xs,
    },
  })
);
