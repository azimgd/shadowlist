import { ScrollView, StyleSheet } from 'react-native';
import { useNavigation } from '@react-navigation/native';
import type { NativeStackNavigationProp } from '@react-navigation/native-stack';
import { Grouped, createStyles } from 'shadowlist-utils/native';
import { EXAMPLE_SECTIONS, type RootStackParamList } from '../routes';

export const HomeScreen = () => {
  const styles = useStyles();
  const navigation =
    useNavigation<NativeStackNavigationProp<RootStackParamList>>();
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
              onPress={() => navigation.navigate(example.route)}
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
