import { memo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';
import { Shadowlist } from 'shadowlist';

export interface GridItem {
  id: string;
  imageUrl: string;
  title: string;
}

export interface GridElement {
  id: string;
  title: string;
  items: GridItem[];
}

interface GridElementProps {
  element: GridElement;
  index: number;
}

const GridItemComponent = memo(({ item }: { item: GridItem }) => {
  return (
    <View style={styles.gridItem}>
      <View style={styles.imageContainer}>
        <Image
          source={{ uri: item.imageUrl }}
          style={styles.image}
          resizeMode="cover"
        />
      </View>
      <Text style={styles.title} numberOfLines={2}>
        {item.title}
      </Text>
    </View>
  );
});

export const GridElement = memo(({ element, index }: GridElementProps) => {
  return (
    <View style={styles.container}>
      <Text style={styles.sectionTitle}>{element.title}</Text>
      <Shadowlist
        data={element.items}
        horizontal
        style={styles.horizontalList}
        renderItem={({ item: gridItem }) => (
          <GridItemComponent item={gridItem} />
        )}
      />
    </View>
  );
});

const styles = StyleSheet.create({
  container: {
    paddingVertical: 12,
    height: 300,
  },
  sectionTitle: {
    color: '#FFFFFF',
    fontSize: 20,
    fontWeight: '700',
    paddingHorizontal: 16,
    marginBottom: 12,
  },
  horizontalList: {
    backgroundColor: '#000000',
  },
  gridItem: {
    width: 180,
    marginLeft: 16,
  },
  imageContainer: {
    width: 180,
    height: 240,
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
  },
});
