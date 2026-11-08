import { memo } from 'react';
import {
  Pressable,
  StyleSheet,
  Text,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLargeText } from '../hooks/useLargeText';
import { createStyles } from '../theme';

export interface FormButtonProps {
  label: string;
  onPress: () => void;
  destructive?: boolean;
  style?: StyleProp<ViewStyle>;
}

/*
 * A full-width button in a card of its own under a form, such as "Remove item". At accessibility
 * text sizes its label wraps to a second line instead of truncating.
 */
export const FormButton = memo(
  ({ label, onPress, destructive = false, style }: FormButtonProps) => {
    const styles = useStyles();
    const largeText = useLargeText();
    return (
      <Pressable
        accessibilityRole="button"
        accessibilityLabel={label}
        onPress={onPress}
        style={({ pressed }) => [
          styles.button,
          pressed && styles.pressed,
          style,
        ]}
      >
        <Text
          style={[styles.label, destructive && styles.destructive]}
          numberOfLines={largeText ? 2 : 1}
        >
          {label}
        </Text>
      </Pressable>
    );
  }
);

const useStyles = createStyles(({ colors, typography, grouped, tapTarget }) =>
  StyleSheet.create({
    button: {
      minHeight: tapTarget + 6,
      paddingHorizontal: grouped.rowInset,
      paddingVertical: 8,
      borderRadius: grouped.radius,
      alignItems: 'center',
      justifyContent: 'center',
      backgroundColor: colors.groupedCell,
    },
    label: {
      ...typography.body,
      color: colors.accent,
      textAlign: 'center',
    },
    destructive: {
      color: colors.red,
    },
    pressed: {
      opacity: 0.55,
    },
  })
);
