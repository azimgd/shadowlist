import type { ReactNode } from 'react';
import { StyleSheet, View, type StyleProp, type ViewStyle } from 'react-native';
import { createStyles } from '../theme';

export interface GroupedCardProps {
  children: ReactNode;
  style?: StyleProp<ViewStyle>;
}

/*
 * The inset-grouped card. Blocks inside it are separated by hairlines, never by a second
 * rounded container. A radius inside a radius stops an iOS list looking native.
 */
export const GroupedCard = ({ children, style }: GroupedCardProps) => {
  const styles = useStyles();
  return <View style={[styles.card, style]}>{children}</View>;
};

const useStyles = createStyles(({ colors, grouped }) =>
  StyleSheet.create({
    card: {
      marginHorizontal: grouped.inset,
      borderRadius: grouped.radius,
      backgroundColor: colors.groupedCell,
      overflow: 'hidden',
    },
  })
);
