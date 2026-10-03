import { ScrollView, StyleSheet } from 'react-native';
import { Grouped, createStyles } from 'shadowlist-utils/native';
import { useNavigation } from '../navigation/Navigation';
import { EXAMPLE_SECTIONS } from '../routes';

export const HomeScreen = () => {
  const styles = useStyles();
  const { select } = useNavigation();
  return (
    <ScrollView
      style={styles.page}
      contentContainerStyle={styles.content}
      contentInsetAdjustmentBehavior="automatic"
    >
      {EXAMPLE_SECTIONS.map((section) => (
        <Grouped.Section key={section.title} title={section.title}>
          {section.examples.map((example, index) => (
            <Grouped.Row
              key={example.route}
              testID={example.route}
              title={example.title}
              subtitle={example.summary}
              subtitleLines={2}
              separated={index > 0}
              onPress={() => select(example.route)}
            />
          ))}
        </Grouped.Section>
      ))}
    </ScrollView>
  );
};

const useStyles = createStyles(({ colors, spacing }) =>
  StyleSheet.create({
    page: {
      flex: 1,
      backgroundColor: colors.groupedBackground,
    },
    content: {
      paddingTop: spacing.xxl,
      paddingBottom: spacing.xxl * 2,
    },
  })
);
