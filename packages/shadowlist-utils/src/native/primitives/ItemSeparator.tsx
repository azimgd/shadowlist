import { memo } from 'react';
import { View, StyleSheet, type StyleProp, type ViewStyle } from 'react-native';
import { createStyles } from '../theme';

export interface ItemSeparatorProps {
  style?: StyleProp<ViewStyle>;
  highlighted?: boolean;
}

/*
 * An inset hairline. Used as a list's ItemSeparatorComponent it hides while a row next to it
 * is highlighted, like a native list's separator.
 */
export const ItemSeparator = memo(
  ({ style, highlighted = false }: ItemSeparatorProps) => {
    const styles = useStyles();
    return (
      <View style={[styles.container, style]}>
        <View style={[styles.line, highlighted && styles.hidden]} />
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    container: {
      backgroundColor: theme.colors.background,
    },
    line: {
      height: StyleSheet.hairlineWidth,
      backgroundColor: theme.colors.separator,
      marginLeft: theme.rowInset,
    },
    hidden: {
      opacity: 0,
    },
  })
);
