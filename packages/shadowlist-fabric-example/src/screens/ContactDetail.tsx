import { ScrollView, StyleSheet, Text, View } from 'react-native';
import {
  Avatar,
  Grouped,
  createStyles,
  type ContactItem,
} from 'shadowlist-utils/native';

export const ContactDetail = ({ contact }: { contact: ContactItem }) => {
  const styles = useStyles();
  return (
    <ScrollView
      style={styles.page}
      contentContainerStyle={styles.content}
      contentInsetAdjustmentBehavior="automatic"
    >
      <View style={styles.hero}>
        <Avatar
          name={contact.name}
          uri={contact.avatarUrl}
          color={contact.avatarColor}
          size={96}
        />
        <Text style={styles.name} accessibilityRole="header">
          {contact.name}
        </Text>
      </View>
      {contact.subtitle ? (
        <Grouped.Card>
          <View style={styles.row} accessible>
            <Text style={styles.rowLabel}>mobile</Text>
            <Text style={styles.rowValue} selectable>
              {contact.subtitle}
            </Text>
          </View>
        </Grouped.Card>
      ) : null}
    </ScrollView>
  );
};

const useStyles = createStyles(({ colors, typography, spacing, grouped }) =>
  StyleSheet.create({
    page: {
      flex: 1,
      backgroundColor: colors.groupedBackground,
    },
    content: {
      paddingVertical: spacing.lg,
      gap: spacing.lg,
    },
    hero: {
      alignItems: 'center',
      gap: spacing.md,
      paddingVertical: spacing.lg,
      paddingHorizontal: spacing.lg,
    },
    name: {
      color: colors.label,
      ...typography.title2,
      textAlign: 'center',
    },
    row: {
      paddingHorizontal: grouped.rowInset,
      paddingVertical: 11,
      gap: 2,
    },
    rowLabel: {
      color: colors.label,
      ...typography.subhead,
    },
    rowValue: {
      color: colors.accent,
      ...typography.body,
    },
  })
);
