import { memo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';

export interface MasonryElement {
  id: string;
  imageUrl: string;
  title: string;
  height: number;
}

interface MasonryElementProps {
  element: MasonryElement;
  index: number;
}

export const MasonryElement = memo(({ element, index }: MasonryElementProps) => {
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
    backgroundColor: '#000000',
    marginBottom: 12,
    paddingHorizontal: 6,
  },
  imageContainer: {
    width: '100%',
    borderRadius: 8,
    overflow: 'hidden',
    backgroundColor: '#2F3336',
    marginBottom: 8,
  },
  image: {
    width: '100%',
    height: '100%',
  },
  title: {
    color: '#FFFFFF',
    fontSize: 14,
    lineHeight: 18,
    paddingHorizontal: 4,
  },
});
