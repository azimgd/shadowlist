import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { ShadowList, type ShadowListProps } from 'shadowlist';
import type {
  NestedItem,
  NestedCard as NestedCardData,
} from 'shadowlist-utils';
import { NestedCard } from './NestedCard';
import { colors, typography, spacing } from '../theme';

export interface NestedRowProps {
  element: NestedItem;
}

const renderNestedCard: ShadowListProps<NestedCardData>['renderElement'] = ({
  element,
}) => <NestedCard element={element} />;

/* One titled row containing a horizontal, virtualized carousel of cards. */
export const NestedRow = memo(({ element }: NestedRowProps) => {
  return (
    <View style={styles.container}>
      <Text style={styles.sectionTitle}>{element.title}</Text>
      <ShadowList
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
    marginBottom: spacing.lg,
    height: 300,
  },
  sectionTitle: {
    color: colors.label,
    ...typography.title3,
    paddingHorizontal: spacing.lg,
    marginBottom: spacing.md,
  },
  horizontalList: {
    backgroundColor: colors.background,
  },
});
