import { memo, useContext } from 'react';
import { StyleSheet, TextInput } from 'react-native';
import { createStyles, useTheme } from '../theme';
import { FormRowStackedContext } from './FormRowContext';

export interface FormInputProps {
  value: string;
  onChange: (text: string) => void;
  placeholder?: string;
  numeric?: boolean;
  keyboardType?: 'default' | 'decimal-pad' | 'numbers-and-punctuation';
  autoCapitalize?: 'none' | 'sentences';
  accessibilityLabel: string;
}

/*
 * A form row's text field. It has no box of its own because the row is the box. It aligns to
 * the trailing edge like every other value, or to the leading one in a stacked row.
 */
export const FormInput = memo(
  ({
    value,
    onChange,
    placeholder,
    numeric = false,
    keyboardType,
    autoCapitalize,
    accessibilityLabel,
  }: FormInputProps) => {
    const styles = useStyles();
    const { colors } = useTheme();
    const stacked = useContext(FormRowStackedContext);
    return (
      <TextInput
        value={value}
        onChangeText={onChange}
        placeholder={placeholder}
        placeholderTextColor={colors.tertiaryLabel}
        keyboardType={keyboardType ?? (numeric ? 'decimal-pad' : 'default')}
        autoCapitalize={autoCapitalize ?? (numeric ? 'none' : 'sentences')}
        autoCorrect={!numeric}
        textAlign={stacked ? 'left' : 'right'}
        style={[styles.input, numeric && styles.numeric]}
        accessibilityLabel={accessibilityLabel}
        clearButtonMode="while-editing"
      />
    );
  }
);

const useStyles = createStyles(({ colors, typography }) =>
  StyleSheet.create({
    input: {
      fontSize: typography.body.fontSize,
      letterSpacing: typography.body.letterSpacing,
      color: colors.label,
      flex: 1,
      minHeight: 38,
      paddingVertical: 0,
    },
    numeric: {
      fontVariant: ['tabular-nums'],
    },
  })
);
