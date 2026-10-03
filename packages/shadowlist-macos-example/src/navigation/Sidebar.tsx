import { Pressable, ScrollView, StyleSheet, Text, View } from 'react-native';
import { semantic, useTheme } from 'shadowlist-utils/native';
import { EXAMPLE_SECTIONS, type ExampleRoute } from '../routes';
import { useNavigation } from './Navigation';
import { useHover } from './useHover';

interface SidebarItem {
  route: 'Home' | ExampleRoute;
  title: string;
}

interface SidebarSection {
  title: string;
  items: SidebarItem[];
}

const SECTIONS: SidebarSection[] = [
  { title: '', items: [{ route: 'Home', title: 'All Examples' }] },
  ...EXAMPLE_SECTIONS.map((section) => ({
    title: section.title,
    items: section.examples.map(({ route, title }) => ({ route, title })),
  })),
];

interface SidebarRowProps {
  title: string;
  selected: boolean;
  onPress: () => void;
}

/*
 * A source-list row: AppKit draws the chosen row as a rounded rect in the selection color and
 * shows a faint gray rect under the pointer otherwise. `selectedContentBackgroundColor` is the
 * accent-tinted variant that dims to gray when the window is not key, which is the behavior
 * that makes a list feel like it belongs to a Mac app.
 */
function SidebarRow({ title, selected, onPress }: SidebarRowProps) {
  const { colors } = useTheme();
  const [hovered, hover] = useHover();
  return (
    <Pressable
      onPress={onPress}
      accessibilityRole="button"
      accessibilityState={{ selected }}
      {...hover}
      style={({ pressed }) => [
        styles.row,
        selected && { backgroundColor: colors.accentSoft },
        !selected && hovered && { backgroundColor: colors.fill },
        pressed && !selected && { backgroundColor: colors.tertiaryLabel },
      ]}
    >
      <Text
        numberOfLines={1}
        style={[
          styles.rowLabel,
          { color: selected ? colors.onAccent : colors.label },
        ]}
      >
        {title}
      </Text>
    </Pressable>
  );
}

export function Sidebar() {
  const { colors } = useTheme();
  const { stack, select } = useNavigation();
  const current = stack[0]?.route.name;
  return (
    <View style={styles.sidebar}>
      <View style={styles.header}>
        <Text style={[styles.title, { color: colors.secondaryLabel }]}>
          ShadowList
        </Text>
      </View>
      <ScrollView style={styles.scroll}>
        {SECTIONS.map((section) => (
          <View key={section.title}>
            {section.title ? (
              <Text
                style={[styles.sectionHeader, { color: colors.tertiaryLabel }]}
              >
                {section.title}
              </Text>
            ) : null}
            {section.items.map((item) => (
              <SidebarRow
                key={item.route}
                title={item.title}
                selected={item.route === current}
                onPress={() => select(item.route)}
              />
            ))}
          </View>
        ))}
      </ScrollView>
    </View>
  );
}

const styles = StyleSheet.create({
  sidebar: {
    width: 220,
    // The recessed page color, which is what a source list sits on.
    backgroundColor: semantic(
      'underPageBackgroundColor',
      'windowBackgroundColor'
    ),
  },
  header: {
    height: 38,
    justifyContent: 'center',
    paddingHorizontal: 12,
  },
  title: { fontSize: 11, fontWeight: '600', letterSpacing: 0.4 },
  scroll: { flex: 1, paddingHorizontal: 6 },
  sectionHeader: {
    fontSize: 11,
    fontWeight: '600',
    paddingHorizontal: 6,
    paddingTop: 10,
    paddingBottom: 4,
  },
  row: {
    height: 24,
    paddingHorizontal: 8,
    borderRadius: 5,
    justifyContent: 'center',
    marginBottom: 1,
  },
  rowLabel: { fontSize: 13 },
});
