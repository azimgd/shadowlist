import { View, StyleSheet } from 'react-native';
import {
  EmptyState,
  PillButton,
  Spinner,
  createStyles,
} from 'shadowlist-utils/native';

interface QueryStatusProps {
  error: Error | null;
  onRetry: () => void;
}

/*
 * Shown before a list screen has its first page: a spinner, or the error with a retry. Screens
 * mount their list only once data is in. Props that act on the first layout see real rows.
 */
export const QueryStatus = ({ error, onRetry }: QueryStatusProps) => {
  const styles = useStyles();
  return (
    <View style={styles.container}>
      {error ? (
        <>
          <EmptyState title={error.message} />
          <PillButton label="Try again" variant="tinted" onPress={onRetry} />
        </>
      ) : (
        <Spinner />
      )}
    </View>
  );
};

const useStyles = createStyles(({ colors }) =>
  StyleSheet.create({
    container: {
      flex: 1,
      alignItems: 'center',
      justifyContent: 'center',
      gap: 12,
      padding: 24,
      backgroundColor: colors.background,
    },
  })
);
