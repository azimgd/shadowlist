import { memo } from 'react';
import { StyleSheet, Text, type StyleProp, type TextStyle } from 'react-native';
import { createStyles } from '../theme';

export interface GroupedCaptionProps {
  title: string;
  style?: StyleProp<TextStyle>;
}

// The uppercase caption over a card, lined up with the text of the card's rows.
export const GroupedCaption = memo(({ title, style }: GroupedCaptionProps) => {
  const styles = useStyles();
  return (
    <Text style={[styles.caption, style]} accessibilityRole="header">
      {title.toUpperCase()}
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
