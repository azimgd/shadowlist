import { Pressable, ScrollView, StyleSheet, Text, View } from 'react-native';
import { useNavigation } from '@react-navigation/native';
import type { NativeStackNavigationProp } from '@react-navigation/native-stack';
import { ChevronIcon, createStyles, useTheme } from 'shadowlist-utils/native';
import { GROUP_RADIUS, groupedColors } from './appTheme';
import {
  EXAMPLE_SECTIONS,
  type Example,
  type RootStackParamList,
} from './routes';

export const HomeScreen = () => {
  const styles = useStyles();
  return (
    <ScrollView
      style={styles.page}
      contentContainerStyle={styles.content}
      contentInsetAdjustmentBehavior="automatic"
    >
      {EXAMPLE_SECTIONS.map((section) => (
        <View key={section.title} style={styles.section}>
          <Text style={styles.sectionTitle} accessibilityRole="header">
            {section.title}
          </Text>
          <View style={styles.group}>
            {section.examples.map((example, index) => (
              <ExampleRow
                key={example.route}
                example={example}
                separated={index > 0}
              />
            ))}
          </View>
        </View>
      ))}
    </ScrollView>
  );
};

const ExampleRow = ({
  example,
  separated,
}: {
  example: Example;
  separated: boolean;
}) => {
  const styles = useStyles();
  const { colors } = useTheme();
  const navigation =
    useNavigation<NativeStackNavigationProp<RootStackParamList>>();
  return (
    <Pressable
      testID={example.route}
      accessibilityRole="button"
      accessibilityLabel={example.title}
      accessibilityHint={example.summary}
      onPress={() => navigation.navigate(example.route)}
      style={({ pressed }) => [styles.row, pressed && styles.rowPressed]}
    >
      <View style={[styles.rowBody, separated && styles.separator]}>
        <View style={styles.rowText}>
          <Text style={styles.title}>{example.title}</Text>
          <Text style={styles.summary}>{example.summary}</Text>
        </View>
        <ChevronIcon
          direction="right"
          color={colors.tertiaryLabel}
          size={18}
          strokeWidth={2}
        />
      </View>
    </Pressable>
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
      paddingHorizontal: spacing.lg,
      paddingBottom: spacing.xxl * 2,
    },
    section: {
      marginTop: spacing.xxl,
    },
    sectionTitle: {
      color: colors.secondaryLabel,
      ...typography.footnote,
      textTransform: 'uppercase',
      marginHorizontal: spacing.lg,
      marginBottom: spacing.sm - 2,
    },
    group: {
      borderRadius: GROUP_RADIUS,
      overflow: 'hidden',
      backgroundColor: grouped.cell,
    },
    row: {
      paddingLeft: spacing.lg,
      backgroundColor: grouped.cell,
    },
    rowPressed: {
      backgroundColor: colors.fill,
    },
    rowBody: {
      flexDirection: 'row',
      alignItems: 'center',
      minHeight: 44,
      paddingVertical: 11,
      paddingRight: spacing.md,
      gap: spacing.sm,
    },
    separator: {
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: colors.separator,
    },
    rowText: {
      flex: 1,
      gap: 2,
    },
    title: {
      color: colors.label,
      ...typography.body,
    },
    summary: {
      color: colors.secondaryLabel,
      ...typography.subhead,
    },
  });
});
