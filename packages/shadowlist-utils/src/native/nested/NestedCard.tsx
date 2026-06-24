import { memo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';
import type { NestedCard as NestedCardData } from 'shadowlist-utils';
import { colors, typography, radius, spacing } from '../theme';

export interface NestedCardProps {
  element: NestedCardData;
}

export const NestedCard = memo(({ element }: NestedCardProps) => {
  return (
    <View style={styles.nestedElement}>
      <View style={styles.imageContainer}>
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
  nestedElement: {
    width: 180,
    marginLeft: spacing.lg,
  },
  imageContainer: {
    width: 180,
    height: 220,
    borderRadius: radius.md,
    overflow: 'hidden',
    backgroundColor: colors.elevated2,
    marginBottom: spacing.sm,
  },
  image: {
    width: '100%',
    height: '100%',
  },
  title: {
    color: colors.label,
    ...typography.subhead,
  },
});
