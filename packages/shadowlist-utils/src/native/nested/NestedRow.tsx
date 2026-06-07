import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { Shadowlist, type ShadowlistProps } from 'shadowlist';
import type {
  NestedItem,
  NestedCard as NestedCardData,
} from 'shadowlist-utils';
import { NestedCard } from './NestedCard';
import { colors, typography } from '../theme';

export interface NestedRowProps {
  element: NestedItem;
}

const renderNestedCard: ShadowlistProps<NestedCardData>['renderElement'] = ({
  element,
}) => <NestedCard element={element} />;

/* One titled row containing a horizontal, virtualized carousel of cards. */
export const NestedRow = memo(({ element }: NestedRowProps) => {
  return (
    <View style={styles.container}>
      <Text style={styles.sectionTitle}>{element.title}</Text>
      <Shadowlist
        data={element.elements}
        horizontal
        style={styles.horizontalList}
        renderElement={renderNestedCard}
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
});
