import { ScrollView, StyleSheet, Text, View } from 'react-native';
import type { NativeStackScreenProps } from '@react-navigation/native-stack';
import { Avatar, createStyles } from 'shadowlist-utils/native';
import { GROUP_RADIUS, groupedColors } from './appTheme';
import type { RootStackParamList } from './routes';

type Props = NativeStackScreenProps<RootStackParamList, 'ContactDetail'>;

export const ContactDetailScreen = ({ route }: Props) => {
  const styles = useStyles();
  const { contact } = route.params;
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
        <View style={styles.group}>
          <View style={styles.row} accessible>
            <Text style={styles.rowLabel}>mobile</Text>
            <Text style={styles.rowValue} selectable>
              {contact.subtitle}
            </Text>
          </View>
        </View>
      ) : null}
    </ScrollView>
  );
};

const useStyles = createStyles((theme) => {
  const { colors, typography, spacing } = theme;
  const grouped = groupedColors(theme);
  return StyleSheet.create({
    page: {
      flex: 1,
      backgroundColor: grouped.page,
    },
    content: {
      padding: spacing.lg,
    },
    hero: {
      alignItems: 'center',
      gap: spacing.md,
      paddingVertical: spacing.lg,
    },
    name: {
      color: colors.label,
      ...typography.title2,
      textAlign: 'center',
    },
    group: {
      marginTop: spacing.lg,
      borderRadius: GROUP_RADIUS,
      backgroundColor: grouped.cell,
    },
    row: {
      paddingHorizontal: spacing.lg,
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
  });
});
