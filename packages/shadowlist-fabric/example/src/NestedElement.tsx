import { memo } from 'react';
import { View, Text, Image, StyleSheet } from 'react-native';
import { Shadowlist } from 'shadowlist';
import { colors, typography, radius } from './theme';

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

const NestedElementChildComponent = memo(
  ({ element }: { element: NestedElementChild }) => {
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
  }
);

export const NestedElement = memo(({ element }: NestedElementProps) => {
  return (
    <View style={styles.container}>
      <Text style={styles.sectionTitle}>{element.title}</Text>
      <Shadowlist
        data={element.elements}
        horizontal
        style={styles.horizontalList}
        renderElement={({ element: nestedElementChild }) => (
          <NestedElementChildComponent element={nestedElementChild} />
        )}
      />
    </View>
  );
});

const styles = StyleSheet.create({
  container: {
    marginBottom: 16,
    height: 300,
  },
  sectionTitle: {
    color: colors.label,
    ...typography.title3,
    paddingHorizontal: 16,
    marginBottom: 12,
  },
  horizontalList: {
    backgroundColor: colors.background,
  },
  nestedElement: {
    width: 180,
    marginLeft: 16,
  },
  imageContainer: {
    width: 180,
    height: 220,
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
  },
});
