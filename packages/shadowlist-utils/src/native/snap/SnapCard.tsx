import { memo } from 'react';
import { View, useWindowDimensions, StyleSheet } from 'react-native';
import type { SnapItem } from 'shadowlist-utils';
import { radius } from '../theme';

export interface SnapCardProps {
  element: SnapItem;
}

// Each card fills a quarter of the viewport so four are visible at rest.
export const SnapCard = memo(({ element }: SnapCardProps) => {
  const { height } = useWindowDimensions();
  return (
    <View style={{ height: height / 4 }}>
      <View style={[styles.card, { backgroundColor: element.accent }]} />
    </View>
  );
});

const styles = StyleSheet.create({
  card: {
    flex: 1,
    margin: 8,
    borderRadius: radius.lg,
  },
});
