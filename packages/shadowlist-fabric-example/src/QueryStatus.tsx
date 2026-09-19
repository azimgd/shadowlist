import { View, Text, Pressable, StyleSheet } from 'react-native';
import { Spinner, createStyles } from 'shadowlist-utils/native';

interface QueryStatusProps {
  error: Error | null;
  onRetry: () => void;
}

/*
 * What a list screen shows before its first page exists: a spinner, or the error with a
 * retry. Screens mount their list only once data is in, so props that act on the first
 * layout (containerOffsetIndex, a tree's initial expansion) see the real rows.
 */
export const QueryStatus = ({ error, onRetry }: QueryStatusProps) => {
  const styles = useStyles();
  return (
    <View style={styles.container}>
      {error ? (
        <>
          <Text style={styles.message}>{error.message}</Text>
          <Pressable
            accessibilityRole="button"
            onPress={() => onRetry()}
            hitSlop={8}
            style={({ pressed }) => [styles.button, pressed && styles.pressed]}
          >
            <Text style={styles.buttonText}>Try again</Text>
          </Pressable>
        </>
      ) : (
        <Spinner />
      )}
    </View>
  );
};

const useStyles = createStyles(({ colors, typography }) =>
  StyleSheet.create({
    container: {
      flex: 1,
      alignItems: 'center',
      justifyContent: 'center',
      gap: 12,
      padding: 24,
      backgroundColor: colors.background,
    },
    message: {
      color: colors.secondaryLabel,
      textAlign: 'center',
      ...typography.body,
    },
    button: {
      paddingVertical: 6,
      paddingHorizontal: 12,
    },
    pressed: {
      opacity: 0.4,
    },
    buttonText: {
      color: colors.accent,
      ...typography.body,
    },
  })
);
