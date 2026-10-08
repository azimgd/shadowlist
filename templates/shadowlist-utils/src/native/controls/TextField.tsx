import { memo } from 'react';
import {
  StyleSheet,
  TextInput,
  type KeyboardTypeOptions,
  type StyleProp,
  type TextStyle,
} from 'react-native';
import { createStyles, useTheme } from '../theme';

export interface TextFieldProps {
  value: string;
  onChange: (text: string) => void;
  placeholder?: string;
  keyboardType?: KeyboardTypeOptions;
  accessibilityLabel?: string;
  multiline?: boolean;
  style?: StyleProp<TextStyle>;
}

/*
 * A filled text box for a value typed verbatim: no autocorrect, no capitals.
 */
export const TextField = memo(
  ({
    value,
    onChange,
    placeholder,
    keyboardType = 'default',
    accessibilityLabel,
    multiline = false,
    style,
  }: TextFieldProps) => {
    const styles = useStyles();
    const { colors } = useTheme();
    return (
      <TextInput
        value={value}
        onChangeText={onChange}
        placeholder={placeholder}
        placeholderTextColor={colors.secondaryLabel}
        keyboardType={keyboardType}
        multiline={multiline}
        style={[styles.field, multiline && styles.block, style]}
        autoCorrect={false}
        autoCapitalize="none"
        accessibilityLabel={accessibilityLabel ?? placeholder}
      />
    );
  }
);

const useStyles = createStyles(({ colors, typography, fonts }) =>
  StyleSheet.create({
    field: {
      height: 40,
      paddingHorizontal: 12,
      borderRadius: 9,
      backgroundColor: colors.fill,
      fontSize: typography.body.fontSize,
      letterSpacing: typography.body.letterSpacing,
      color: colors.label,
    },
    // Tall enough for a few rows of pasted text.
    block: {
      height: 104,
      paddingTop: 10,
      paddingBottom: 10,
      textAlignVertical: 'top',
      fontFamily: fonts.mono,
      fontSize: 14,
      letterSpacing: -0.6,
    },
  })
);
