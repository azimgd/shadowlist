import { memo, type ReactNode } from 'react';
import {
  Image,
  Pressable,
  StyleSheet,
  Text,
  View,
  useWindowDimensions,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { createStyles } from '../theme';
import { defaultSnapLabels, type SnapLabels } from './labels';
import type { SnapItem } from './types';

export interface SnapCardProps {
  item: SnapItem;
  // Replaces the default image/title content inside the card frame.
  children?: ReactNode;
  // Defaults to a quarter of the window height.
  height?: number;
  onPress?: (item: SnapItem) => void;
  labels?: Partial<SnapLabels>;
  style?: StyleProp<ViewStyle>;
}

export const SnapCard = memo(
  ({ item, children, height, onPress, labels, style }: SnapCardProps) => {
    const styles = useStyles();
    const window = useWindowDimensions();
    const l = useLabels(defaultSnapLabels, labels);
    const { title, subtitle, image, color } = item;
    const hasText = title !== undefined || subtitle !== undefined;
    const accessibilityLabel =
      [title, subtitle].filter(Boolean).join(', ') || image?.alt || l.card;

    const cardStyle = [
      styles.card,
      color !== undefined && { backgroundColor: color },
      style,
    ];
    const content = children ?? (
      <>
        {image !== undefined && (
          <Image
            source={{ uri: image.uri }}
            style={StyleSheet.absoluteFill}
            resizeMode="cover"
          />
        )}
        {hasText && (
          <View style={styles.caption}>
            {title !== undefined && (
              <Text style={styles.title} numberOfLines={2}>
                {title}
              </Text>
            )}
            {subtitle !== undefined && (
              <Text style={styles.subtitle} numberOfLines={2}>
                {subtitle}
              </Text>
            )}
          </View>
        )}
      </>
    );

    return (
      <View style={{ height: height ?? window.height / 4 }}>
        {onPress === undefined ? (
          <View
            style={cardStyle}
            accessible={hasText || image?.alt !== undefined}
            accessibilityLabel={accessibilityLabel}
          >
            {content}
          </View>
        ) : (
          <Pressable
            style={cardStyle}
            accessibilityRole="button"
            accessibilityLabel={accessibilityLabel}
            onPress={() => onPress(item)}
          >
            {content}
          </Pressable>
        )}
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    card: {
      flex: 1,
      margin: theme.spacing.sm,
      borderRadius: theme.radius.lg,
      overflow: 'hidden',
      justifyContent: 'flex-end',
      backgroundColor: theme.colors.elevated,
    },
    caption: {
      padding: theme.spacing.lg,
      backgroundColor: 'rgba(0,0,0,0.35)',
    },
    title: {
      color: '#FFFFFF',
      ...theme.typography.title3,
    },
    subtitle: {
      color: 'rgba(255,255,255,0.8)',
      ...theme.typography.subhead,
    },
  })
);
