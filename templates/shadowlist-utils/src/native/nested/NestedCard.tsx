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
import { createStyles } from '../theme';
import type { NestedCardItem } from './types';

export interface NestedCardProps {
  item: NestedCardItem;
  onPress?: (item: NestedCardItem) => void;
  style?: StyleProp<ViewStyle>;
  imageStyle?: StyleProp<ImageStyle>;
}

export const NestedCard = memo(
  ({ item, onPress, style, imageStyle }: NestedCardProps) => {
    const styles = useStyles();
    const content = (
      <>
        <View style={styles.imageFrame}>
          <Image
            source={{ uri: item.image.uri }}
            style={[styles.image, imageStyle]}
            resizeMode="cover"
            accessible={item.image.alt !== undefined}
            accessibilityLabel={item.image.alt}
          />
        </View>
        <Text style={styles.title} numberOfLines={2}>
          {item.title}
        </Text>
      </>
    );

    if (onPress === undefined) {
      return <View style={[styles.card, style]}>{content}</View>;
    }
    return (
      <Pressable
        style={[styles.card, style]}
        accessibilityRole="button"
        accessibilityLabel={item.title}
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
      width: 180,
      marginLeft: theme.spacing.lg,
    },
    imageFrame: {
      width: 180,
      height: 220,
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
    },
  })
);
