import { memo } from 'react';
import { StyleSheet, Text, type StyleProp, type TextStyle } from 'react-native';
import { createStyles, useTheme } from '../theme';

export interface GroupedCaptionProps {
  title: string;
  style?: StyleProp<TextStyle>;
}

/*
 * The caption over a card, lined up with the text of the card's rows. It is set in capitals
 * unless the theme's grouped.captionCase is none.
 */
export const GroupedCaption = memo(({ title, style }: GroupedCaptionProps) => {
  const styles = useStyles();
  const { grouped } = useTheme();
  return (
    <Text style={[styles.caption, style]} accessibilityRole="header">
      {grouped.captionCase === 'upper' ? title.toUpperCase() : title}
    </Text>
  );
});

const useStyles = createStyles(({ colors, typography, spacing, grouped }) =>
  StyleSheet.create({
    caption: {
      ...typography.footnote,
      color: colors.secondaryLabel,
      marginHorizontal: grouped.inset + grouped.rowInset,
      marginBottom: spacing.sm - 2,
    },
  })
);
