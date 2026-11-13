import { memo, useCallback, useMemo } from 'react';
import {
  StyleSheet,
  Text,
  View,
  useWindowDimensions,
  type StyleProp,
  type TextStyle,
  type ViewStyle,
} from 'react-native';
import { ShadowList, type ShadowListProps } from 'shadowlist';
import { createStyles, useTheme } from '../theme';
import { getCardHeight } from './cardHeight';
import { NestedCard } from './NestedCard';
import type { NestedCardItem, NestedItem } from './types';

export interface NestedRowProps {
  item: NestedItem;
  onPressCard?: (card: NestedCardItem, row: NestedItem) => void;
  style?: StyleProp<ViewStyle>;
  titleStyle?: StyleProp<TextStyle>;
}

type RenderCard = ShadowListProps<NestedCardItem>['renderItem'];

const renderCard: RenderCard = ({ item }) => <NestedCard item={item} />;

export const NestedRow = memo(
  ({ item, onPressCard, style, titleStyle }: NestedRowProps) => {
    const styles = useStyles();
    const theme = useTheme();
    const { fontScale } = useWindowDimensions();
    // The list is as tall as a card, which grows with the reader's text size.
    const listStyle = useMemo(
      () => [styles.list, { height: getCardHeight(theme, fontScale) }],
      [styles, theme, fontScale]
    );
    const renderPressableCard = useCallback<RenderCard>(
      ({ item: card }) => (
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
          style={listStyle}
          renderItem={
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
