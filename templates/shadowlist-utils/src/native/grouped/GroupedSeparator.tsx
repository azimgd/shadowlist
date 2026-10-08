import { memo } from 'react';
import { StyleSheet, View, type StyleProp, type ViewStyle } from 'react-native';
import { createStyles, useTheme } from '../theme';

export interface GroupedSeparatorProps {
  inset?: number;
  style?: StyleProp<ViewStyle>;
}

export const GroupedSeparator = memo(
  ({ inset, style }: GroupedSeparatorProps) => {
    const styles = useStyles();
    const { grouped } = useTheme();
    return (
      <View
        style={[styles.line, { marginLeft: inset ?? grouped.rowInset }, style]}
      />
    );
  }
);

const useStyles = createStyles(({ colors }) =>
  StyleSheet.create({
    line: {
      height: StyleSheet.hairlineWidth,
      backgroundColor: colors.separator,
    },
  })
);
