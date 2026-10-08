import { Children, isValidElement, type ReactNode } from 'react';
import { StyleSheet, View, type StyleProp, type ViewStyle } from 'react-native';
import { createStyles } from '../theme';

export interface FormCardProps {
  children: ReactNode;
  style?: StyleProp<ViewStyle>;
}

/*
 * The card of a form. It draws the hairlines between its rows itself. A row that is left out
 * as a falsy child leaves no separator behind.
 */
export const FormCard = ({ children, style }: FormCardProps) => {
  const styles = useStyles();
  const rows = Children.toArray(children).filter(isValidElement);
  return (
    <View style={[styles.card, style]}>
      {rows.map((row, index) => (
        <View key={row.key ?? index}>
          {index > 0 ? <View style={styles.separator} /> : null}
          {row}
        </View>
      ))}
    </View>
  );
};

const useStyles = createStyles(({ colors, grouped }) =>
  StyleSheet.create({
    card: {
      borderRadius: grouped.radius,
      backgroundColor: colors.groupedCell,
      overflow: 'hidden',
    },
    separator: {
      height: StyleSheet.hairlineWidth,
      marginLeft: grouped.rowInset,
      backgroundColor: colors.separator,
    },
  })
);
