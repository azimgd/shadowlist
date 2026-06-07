import 'react-native-gesture-handler';
import { enableScreens } from 'react-native-screens';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { NavigationContainer, DefaultTheme } from '@react-navigation/native';
import { createDrawerNavigator } from '@react-navigation/drawer';
import { FeedScreen } from './FeedScreen';
import { ChatScreen } from './ChatScreen';
import { ActivityScreen } from './ActivityScreen';
import { NestedScreen } from './NestedScreen';
import { MasonryScreen } from './MasonryScreen';
import { ContactsScreen } from './ContactsScreen';
import { SectionListScreen } from './SectionListScreen';
import { ReorderScreen } from './ReorderScreen';
import { TreeScreen } from './TreeScreen';
import { PollScreen } from './PollScreen';
import { SnapScreen } from './SnapScreen';
import { colors, typography } from 'shadowlist-utils/native';

enableScreens();

const Drawer = createDrawerNavigator();

// Pure-black navigation theme so there is no off-black seam between the system
// chrome and the lists.
const navTheme = {
  ...DefaultTheme,
  colors: {
    ...DefaultTheme.colors,
    background: colors.background,
    card: colors.background,
    text: colors.label,
    border: colors.separator,
    primary: colors.accent,
  },
};

export default function App() {
  return (
    <SafeAreaProvider>
      <NavigationContainer theme={navTheme}>
        <Drawer.Navigator
          initialRouteName="Feed"
          screenOptions={{
            // The in-list large title carries the screen name, so the inline
            // nav-bar title is suppressed to avoid showing it twice.
            headerTitle: '',
            headerShadowVisible: false,
            headerTintColor: colors.label,
            headerStyle: {
              backgroundColor: colors.background,
            },
            drawerStyle: {
              backgroundColor: colors.background,
            },
            drawerActiveTintColor: colors.accent,
            drawerInactiveTintColor: colors.secondaryLabel,
            drawerActiveBackgroundColor: colors.accentSoft,
            drawerLabelStyle: typography.body,
          }}
        >
          <Drawer.Screen
            name="Feed"
            component={FeedScreen}
            options={{ title: 'Feed' }}
          />
          <Drawer.Screen
            name="Chat"
            component={ChatScreen}
            options={{ title: 'Chat', headerTitle: 'Chat' }}
          />
          <Drawer.Screen
            name="Activity"
            component={ActivityScreen}
            options={{ title: 'Activity' }}
          />
          <Drawer.Screen
            name="Nested"
            component={NestedScreen}
            options={{ title: 'Nested' }}
          />
          <Drawer.Screen
            name="Masonry"
            component={MasonryScreen}
            options={{ title: 'Masonry' }}
          />
          <Drawer.Screen
            name="Contacts"
            component={ContactsScreen}
            options={{ title: 'Contacts' }}
          />
          <Drawer.Screen
            name="SectionList"
            component={SectionListScreen}
            options={{ title: 'Section List' }}
          />
          <Drawer.Screen
            name="Reorder"
            component={ReorderScreen}
            options={{ title: 'Reorder' }}
          />
          <Drawer.Screen
            name="Tree"
            component={TreeScreen}
            options={{ title: 'Tree' }}
          />
          <Drawer.Screen
            name="Poll"
            component={PollScreen}
            options={{ title: 'Poll' }}
          />
          <Drawer.Screen
            name="Snap"
            component={SnapScreen}
            options={{ title: 'Snap' }}
          />
        </Drawer.Navigator>
      </NavigationContainer>
    </SafeAreaProvider>
  );
}
