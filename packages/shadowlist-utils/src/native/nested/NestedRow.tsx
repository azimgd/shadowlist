import { memo, useCallback } from 'react';
import {
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type TextStyle,
  type ViewStyle,
} from 'react-native';
import { ShadowList, type ShadowListProps } from 'shadowlist';
import { createStyles } from '../theme';
import { NestedCard } from './NestedCard';
import type { NestedCardItem, NestedItem } from './types';

export interface NestedRowProps {
  item: NestedItem;
  onPressCard?: (card: NestedCardItem, row: NestedItem) => void;
  style?: StyleProp<ViewStyle>;
  titleStyle?: StyleProp<TextStyle>;
}

type RenderCard = ShadowListProps<NestedCardItem>['renderElement'];

const renderCard: RenderCard = ({ element }) => <NestedCard item={element} />;

export const NestedRow = memo(
  ({ item, onPressCard, style, titleStyle }: NestedRowProps) => {
    const styles = useStyles();
    const renderPressableCard = useCallback<RenderCard>(
      ({ element: card }) => (
        <NestedCard item={card} onPress={() => onPressCard?.(card, item)} />
      ),
      [onPressCard, item]
    );

    return (
      <View style={[styles.row, style]}>
        <Text style={[styles.title, titleStyle]} accessibilityRole="header">
          {item.title}
        </Text>
        <ShadowList
          data={item.cards}
          horizontal
          style={styles.list}
          renderElement={
            onPressCard === undefined ? renderCard : renderPressableCard
          }
        />
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    row: {
      marginBottom: theme.spacing.lg,
      height: 300,
    },
    title: {
      color: theme.colors.label,
      ...theme.typography.title3,
      paddingHorizontal: theme.spacing.lg,
      marginBottom: theme.spacing.md,
    },
    list: {
      backgroundColor: theme.colors.background,
    },
  })
);
