import type { ReactNode } from 'react';
import { StyleSheet, View, type StyleProp, type ViewStyle } from 'react-native';
import { createStyles } from '../theme';

export interface ChipRowProps {
  children: ReactNode;
  style?: StyleProp<ViewStyle>;
}

/*
 * A row of chips that wraps onto the next line instead of running off the edge.
 */
export const ChipRow = ({ children, style }: ChipRowProps) => {
  const styles = useStyles();
  return <View style={[styles.row, style]}>{children}</View>;
};

const useStyles = createStyles(({ spacing }) =>
  StyleSheet.create({
    row: {
      flexDirection: 'row',
      flexWrap: 'wrap',
      gap: spacing.sm,
    },
  })
);
