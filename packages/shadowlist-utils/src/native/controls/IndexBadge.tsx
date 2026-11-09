import { memo } from 'react';
import {
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles } from '../theme';

export interface IndexBadgeProps {
  index: number;
  style?: StyleProp<ViewStyle>;
}

/*
 * A position in an ordered list of rules, as a small tinted square.
 */
export const IndexBadge = memo(({ index, style }: IndexBadgeProps) => {
  const styles = useStyles();
  return (
    <View style={[styles.badge, style]}>
      <Text style={styles.text}>{index}</Text>
    </View>
  );
});

const useStyles = createStyles(({ colors, fontWeight }) =>
  StyleSheet.create({
    badge: {
      width: 22,
      height: 22,
      borderRadius: 6,
      alignItems: 'center',
      justifyContent: 'center',
      backgroundColor: colors.accentSoft,
    },
    text: {
      fontSize: 12,
      fontWeight: fontWeight.semibold,
      fontVariant: ['tabular-nums'],
      color: colors.accent,
    },
  })
);
