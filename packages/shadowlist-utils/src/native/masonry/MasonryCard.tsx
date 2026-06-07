import { memo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';
import type { MasonryItem } from 'shadowlist-utils';
import { colors, typography, radius } from '../theme';

export interface MasonryCardProps {
  element: MasonryItem;
}

export const MasonryCard = memo(({ element }: MasonryCardProps) => {
  return (
    <View style={styles.masonryElement}>
      <View style={[styles.imageContainer, { height: element.height }]}>
        <Image
          source={{ uri: element.imageUrl }}
          style={styles.image}
          resizeMode="cover"
        />
      </View>
      <Text style={styles.title} numberOfLines={2}>
        {element.title}
      </Text>
    </View>
  );
});

const styles = StyleSheet.create({
  masonryElement: {
    backgroundColor: colors.background,
    marginBottom: 12,
    paddingHorizontal: 6,
  },
  imageContainer: {
    width: '100%',
    borderRadius: radius.md,
    overflow: 'hidden',
    backgroundColor: colors.elevated2,
    marginBottom: 8,
  },
  image: {
    width: '100%',
    height: '100%',
  },
  title: {
    color: colors.label,
    ...typography.subhead,
    paddingHorizontal: 4,
  },
});
