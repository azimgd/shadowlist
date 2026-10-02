import type { ReactNode } from 'react';
import {
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLargeText } from '../hooks/useLargeText';
import { createStyles } from '../theme';
import { FormRowStackedContext } from './FormRowContext';

export interface FormRowProps {
  label: string;
  children: ReactNode;
  stacked?: boolean;
  style?: StyleProp<ViewStyle>;
}

/*
 * One label-and-value row. The label leads and the control sits at the trailing edge,
 * whatever it is. Every value in a form lines up on one edge. Large text stacks the label
 * over the control.
 */
export const FormRow = ({
  label,
  children,
  stacked = false,
  style,
}: FormRowProps) => {
  const styles = useStyles();
  const largeText = useLargeText();
  const over = stacked || largeText;
  return (
    <View style={[styles.row, over && styles.rowStacked, style]}>
      <Text
        style={[styles.label, over && styles.labelStacked]}
        numberOfLines={2}
      >
        {label}
      </Text>
      <View style={[styles.control, over && styles.controlStacked]}>
        <FormRowStackedContext.Provider value={over}>
          {children}
        </FormRowStackedContext.Provider>
      </View>
    </View>
  );
};

const useStyles = createStyles(({ colors, typography, spacing, grouped }) =>
  StyleSheet.create({
    row: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: spacing.md,
      minHeight: grouped.rowHeight,
      paddingHorizontal: grouped.rowInset,
      paddingVertical: 6,
    },
    rowStacked: {
      flexDirection: 'column',
      alignItems: 'stretch',
      gap: spacing.sm,
      paddingVertical: 10,
    },
    label: {
      ...typography.body,
      color: colors.secondaryLabel,
      flexShrink: 0,
      maxWidth: '50%',
    },
    labelStacked: {
      maxWidth: undefined,
    },
    control: {
      flex: 1,
      minWidth: 0,
      flexDirection: 'row',
      alignItems: 'center',
      justifyContent: 'flex-end',
    },
    controlStacked: {
      justifyContent: 'flex-start',
    },
  })
);
