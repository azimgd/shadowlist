import type { ReactNode } from 'react';
import { StyleSheet, View } from 'react-native';
import { Grouped, createStyles } from 'shadowlist-utils/native';

export interface TemplatesSectionProps {
  title: string;
  children: ReactNode;
  card?: boolean;
}

// One captioned block of the Templates page.
export const TemplatesSection = ({
  title,
  children,
  card = false,
}: TemplatesSectionProps) => {
  const styles = useStyles();
  return (
    <View style={styles.section}>
      <Grouped.Caption title={title} />
      {card ? (
        <Grouped.Card style={styles.card}>{children}</Grouped.Card>
      ) : (
        <View style={styles.content}>{children}</View>
      )}
    </View>
  );
};

const useStyles = createStyles(({ spacing, grouped }) =>
  StyleSheet.create({
    section: {
      marginTop: spacing.xxl,
    },
    content: {
      marginHorizontal: grouped.inset,
      gap: spacing.md,
    },
    card: {
      padding: spacing.md,
      gap: spacing.md,
    },
  })
);
