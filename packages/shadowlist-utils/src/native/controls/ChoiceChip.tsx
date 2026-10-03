import { memo } from 'react';
import {
  Pressable,
  StyleSheet,
  Text,
  type ColorValue,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { CheckIcon } from '../icons';
import { createStyles, useTheme } from '../theme';
import { useChipStyles } from './Chip';

export interface ChoiceChipProps {
  label: string;
  tint: ColorValue;
  selected: boolean;
  onPress: () => void;
  style?: StyleProp<ViewStyle>;
}

// One choice of several, in its own color, ringed and ticked when it is the chosen one.
export const ChoiceChip = memo(
  ({ label, tint, selected, onPress, style }: ChoiceChipProps) => {
    const chipStyles = useChipStyles();
    const styles = useStyles();
    const { colors } = useTheme();
    return (
      <Pressable
        accessibilityRole="button"
        accessibilityLabel={label}
        accessibilityState={{ selected }}
        onPress={onPress}
        style={({ pressed }) => [
          chipStyles.chip,
          styles.choice,
          { backgroundColor: tint },
          selected && styles.selected,
          pressed && chipStyles.pressed,
          style,
        ]}
      >
        {selected ? <CheckIcon size={14} color={colors.label} /> : null}
        <Text style={[chipStyles.label, styles.label]} numberOfLines={1}>
          {label}
        </Text>
      </Pressable>
    );
  }
);

const useStyles = createStyles(({ colors }) =>
  StyleSheet.create({
    choice: {
      borderWidth: 2,
      borderColor: 'transparent',
    },
    selected: {
      borderColor: colors.label,
    },
    label: {
      color: colors.label,
    },
  })
);
