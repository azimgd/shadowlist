import { memo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';
import { Shadowlist } from 'shadowlist';

export interface NestedElementChild {
  id: string;
  imageUrl: string;
  title: string;
}

export interface NestedElement {
  id: string;
  title: string;
  elements: NestedElementChild[];
}

interface NestedElementProps {
  element: NestedElement;
  index: number;
}

const NestedElementChildComponent = memo(({ element }: { element: NestedElementChild }) => {
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

export const NestedElement = memo(({ element, index }: NestedElementProps) => {
  return (
    <View style={styles.container}>
      <Text style={styles.sectionTitle}>{element.title}</Text>
      <Shadowlist
        data={element.elements}
        horizontal
        style={styles.horizontalList}
        renderElement={({ element: nestedElementChild }: { element: NestedElementChild }) => (
          <NestedElementChildComponent element={nestedElementChild} />
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
  nestedElement: {
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
