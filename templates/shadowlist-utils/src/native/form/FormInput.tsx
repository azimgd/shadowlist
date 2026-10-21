import { memo, useContext } from 'react';
import { StyleSheet, TextInput } from 'react-native';
import { createStyles, useTheme } from '../theme';
import { FormRowStackedContext } from './FormRowContext';

export interface FormInputProps {
  value: string;
  onChange: (text: string) => void;
  placeholder?: string;
  numeric?: boolean;
  keyboardType?:
    | 'default'
    | 'decimal-pad'
    | 'numbers-and-punctuation'
    | 'email-address';
  autoCapitalize?: 'none' | 'sentences' | 'words' | 'characters';
  autoCorrect?: boolean;
  returnKeyType?: 'done' | 'next' | 'go' | 'search' | 'send';
  onSubmitEditing?: () => void;
  accessibilityLabel: string;
}

/*
 * A form row's text field. It has no box of its own because the row is the box. It aligns to
 * the trailing edge like every other value, or to the leading one in a stacked row. Text is
 * corrected and capitalized like a sentence unless it is numeric or says otherwise. An email
 * address is neither.
 */
export const FormInput = memo(
  ({
    value,
    onChange,
    placeholder,
    numeric = false,
    keyboardType,
    autoCapitalize,
    autoCorrect,
    returnKeyType,
    onSubmitEditing,
    accessibilityLabel,
  }: FormInputProps) => {
    const styles = useStyles();
    const { colors } = useTheme();
    const stacked = useContext(FormRowStackedContext);
    const isEmail = keyboardType === 'email-address';
    const isLiteral = numeric || isEmail;
    return (
      <TextInput
        value={value}
        onChangeText={onChange}
        placeholder={placeholder}
        placeholderTextColor={colors.tertiaryLabel}
        keyboardType={keyboardType ?? (numeric ? 'decimal-pad' : 'default')}
        autoCapitalize={autoCapitalize ?? (isLiteral ? 'none' : 'sentences')}
        autoCorrect={autoCorrect ?? !isLiteral}
        returnKeyType={returnKeyType}
        onSubmitEditing={onSubmitEditing}
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
