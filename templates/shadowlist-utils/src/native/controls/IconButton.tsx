import { memo, type ReactNode } from 'react';
import {
  Pressable,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles } from '../theme';

export interface IconButtonProps {
  icon: ReactNode;
  onPress: () => void;
  accessibilityLabel: string;
  style?: StyleProp<ViewStyle>;
}

/*
 * A round action with a glyph, sized to sit beside a PillButton.
 */
export const IconButton = memo(
  ({ icon, onPress, accessibilityLabel, style }: IconButtonProps) => {
    const styles = useStyles();
    return (
      <Pressable
        accessibilityRole="button"
        accessibilityLabel={accessibilityLabel}
        hitSlop={4}
        onPress={onPress}
        style={({ pressed }) => [
          styles.button,
          pressed && styles.pressed,
          style,
        ]}
      >
        {icon}
      </Pressable>
    );
  }
);

const useStyles = createStyles(({ colors }) =>
  StyleSheet.create({
    button: {
      width: 40,
      height: 40,
      borderRadius: 20,
      alignItems: 'center',
      justifyContent: 'center',
      backgroundColor: colors.accentSoft,
    },
    pressed: {
      opacity: 0.55,
    },
  })
);
