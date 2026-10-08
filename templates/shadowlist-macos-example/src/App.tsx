import { useEffect, useState } from 'react';
import { StyleSheet, View } from 'react-native';
import { QueryClientProvider } from '@tanstack/react-query';
import { useTheme } from 'shadowlist-utils/native';
import { queryClient } from '@example/queries/queryClient';
import { EXAMPLES, type Route } from './routes';
import {
  NavigationProvider,
  useNavigation,
  type StackEntry,
} from './navigation/Navigation';
import {
  EntryKeyProvider,
  ToolbarMenuProvider,
  useToolbarMenus,
} from './navigation/ToolbarMenu';
import { Sidebar } from './navigation/Sidebar';
import { MoreMenu, Toolbar } from './navigation/Toolbar';
import { HomeScreen } from './screens/HomeScreen';
import { ContactDetail } from '@example/screens/ContactDetail';

const INITIAL_ROUTE: Route = { name: 'Home' };

function titleOf(route: Route): string {
  switch (route.name) {
    case 'Home':
      return 'ShadowList';
    case 'ContactDetail':
      return route.contact.name;
    default:
      return (
        EXAMPLES.find((example) => example.route === route.name)?.title ?? ''
      );
  }
}

function Screen({ route }: { route: Route }) {
  switch (route.name) {
    case 'Home':
      return <HomeScreen />;
    case 'ContactDetail':
      return <ContactDetail contact={route.contact} />;
    default: {
      const Component = EXAMPLES.find(
        (example) => example.route === route.name
      )?.component;
      return Component ? <Component /> : null;
    }
  }
}

/*
 * Every screen in the stack stays mounted, like a native stack. Going back finds the list
 * where it was left. Covered screens sit under the top one and take no input.
 */
function StackEntryView({ entry, top }: { entry: StackEntry; top: boolean }) {
  const { colors } = useTheme();
  return (
    <View
      style={[styles.screen, { backgroundColor: colors.background }]}
      pointerEvents={top ? 'auto' : 'none'}
      accessibilityElementsHidden={!top}
      importantForAccessibility={top ? 'auto' : 'no-hide-descendants'}
    >
      <EntryKeyProvider value={entry.key}>
        <Screen route={entry.route} />
      </EntryKeyProvider>
    </View>
  );
}

function DetailPane() {
  const { stack, goBack } = useNavigation();
  const { menus } = useToolbarMenus();
  const [menuOpen, setMenuOpen] = useState(false);

  const top = stack[stack.length - 1];
  const below = stack[stack.length - 2];
  const menu = top ? menus.get(top.key) : undefined;

  // A new screen never opens with the previous screen's menu showing.
  const topKey = top?.key;
  useEffect(() => setMenuOpen(false), [topKey]);

  return (
    <View style={styles.detail}>
      <Toolbar
        title={top ? titleOf(top.route) : ''}
        backTitle={below ? titleOf(below.route) : undefined}
        onBack={below ? goBack : undefined}
        menuOpen={menuOpen}
        onPressMenu={menu ? () => setMenuOpen((open) => !open) : undefined}
      />
      <View style={styles.stack}>
        {stack.map((entry) => (
          <StackEntryView key={entry.key} entry={entry} top={entry === top} />
        ))}
      </View>
      {menuOpen && menu ? (
        <MoreMenu groups={menu} onClose={() => setMenuOpen(false)} />
      ) : null}
    </View>
  );
}

export default function App() {
  const { colors } = useTheme();
  return (
    <QueryClientProvider client={queryClient}>
      <NavigationProvider initial={INITIAL_ROUTE}>
        <ToolbarMenuProvider>
          <View style={[styles.app, { backgroundColor: colors.background }]}>
            <Sidebar />
            <View
              style={[styles.divider, { backgroundColor: colors.separator }]}
            />
            <DetailPane />
          </View>
        </ToolbarMenuProvider>
      </NavigationProvider>
    </QueryClientProvider>
  );
}

const styles = StyleSheet.create({
  app: { flex: 1, flexDirection: 'row' },
  divider: { width: StyleSheet.hairlineWidth },
  detail: { flex: 1 },
  stack: { flex: 1 },
  screen: { ...StyleSheet.absoluteFillObject },
});
