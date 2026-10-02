import { memo } from 'react';
import { StyleSheet, Text } from 'react-native';
import { createStyles } from '../theme';

export interface FormValueProps {
  text: string;
  accessibilityLabel?: string;
}

// A value the form shows but does not take, such as a total worked out from other rows.
export const FormValue = memo(
  ({ text, accessibilityLabel }: FormValueProps) => {
    const styles = useStyles();
    return (
      <Text
        style={styles.value}
        numberOfLines={1}
        accessibilityLabel={accessibilityLabel}
      >
        {text === '' ? '—' : text}
      </Text>
    );
  }
);

const useStyles = createStyles(({ colors, typography }) =>
  StyleSheet.create({
    value: {
      ...typography.body,
      fontVariant: ['tabular-nums'],
      flexShrink: 1,
      color: colors.secondaryLabel,
      textAlign: 'right',
    },
  })
);
