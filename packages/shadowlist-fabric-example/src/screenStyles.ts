import { StyleSheet } from 'react-native';
import { createStyles } from 'shadowlist-utils/native';

export const useScreenStyles = createStyles(({ colors }) =>
  StyleSheet.create({
    container: {
      flex: 1,
      backgroundColor: colors.background,
    },
    list: {
      flex: 1,
      backgroundColor: colors.background,
    },
  })
);
