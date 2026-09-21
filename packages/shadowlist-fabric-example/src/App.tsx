import 'react-native-gesture-handler';
import { useCallback, useMemo, useState } from 'react';
import { enableScreens } from 'react-native-screens';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import {
  NavigationContainer,
  DarkTheme,
  DefaultTheme,
} from '@react-navigation/native';
import {
  createDrawerNavigator,
  type DrawerContentComponentProps,
} from '@react-navigation/drawer';
import { FeedScreen } from './FeedScreen';
import { FeedNativeScreen } from './FeedNativeScreen';
import { ChatScreen } from './ChatScreen';
import { ChatNativeScreen } from './ChatNativeScreen';
import { AssistantScreen } from './AssistantScreen';
import { ActivityScreen } from './ActivityScreen';
import { NestedScreen } from './NestedScreen';
import { NestedNativeScreen } from './NestedNativeScreen';
import { MasonryScreen } from './MasonryScreen';
import { ContactsScreen } from './ContactsScreen';
import { SectionListScreen } from './SectionListScreen';
import { ReorderScreen } from './ReorderScreen';
import { TreeScreen } from './TreeScreen';
import { SnapScreen } from './SnapScreen';
import { DrawerContent, type ThemePreference } from './DrawerContent';
import { ThemeProvider, darkTheme, lightTheme } from 'shadowlist-utils/native';
import { QueryClientProvider } from '@tanstack/react-query';
import { queryClient } from './queries/queryClient';
import {
  Appearance,
  Platform,
  Settings,
  StatusBar,
  useColorScheme,
} from 'react-native';
import { network } from './api/network';

enableScreens();

/*
 * Launch-argument overrides for scripted device runs (iOS reads them from NSUserDefaults):
 * `-SLRoute Chat` opens that screen, `-SLLatency 0,0` sets the fake network's latency
 * range in ms, `-SLSendFailureRate 0.5` fails that share of chat sends, `-SLTheme light` picks
 * the appearance (dark by default).
 */
function launchSetting(key: string): string | undefined {
  if (Platform.OS !== 'ios') return undefined;
  const value: unknown = Settings.get(key);
  return value == null ? undefined : String(value);
}

const LAUNCH_ROUTE = launchSetting('SLRoute') ?? 'Feed';
const launchLatency = launchSetting('SLLatency')?.split(',').map(Number);
if (launchLatency?.length === 2 && launchLatency.every(Number.isFinite)) {
  network.latencyMs = { min: launchLatency[0]!, max: launchLatency[1]! };
}
const launchFailureRate = Number(launchSetting('SLSendFailureRate'));
if (launchFailureRate >= 0 && launchFailureRate <= 1) {
  network.sendFailureRate = launchFailureRate;
}
const launchTheme = launchSetting('SLTheme');
const LAUNCH_THEME: ThemePreference =
  launchTheme === 'light' || launchTheme === 'system' ? launchTheme : 'dark';

// Native chrome (keyboard, scroll indicators, refresh control) follows the chosen appearance.
function applyColorScheme(preference: ThemePreference) {
  Appearance.setColorScheme(
    preference === 'system' ? 'unspecified' : preference
  );
}
applyColorScheme(LAUNCH_THEME);

const Drawer = createDrawerNavigator();

export default function App() {
  const [themePreference, setThemePreference] =
    useState<ThemePreference>(LAUNCH_THEME);
  const systemScheme = useColorScheme();
  const dark =
    themePreference === 'system'
      ? systemScheme !== 'light'
      : themePreference === 'dark';
  const theme = dark ? darkTheme : lightTheme;

  const handleThemePreferenceChange = useCallback(
    (preference: ThemePreference) => {
      applyColorScheme(preference);
      setThemePreference(preference);
    },
    []
  );

  const navTheme = useMemo(() => {
    const base = dark ? DarkTheme : DefaultTheme;
    const { colors } = theme;
    return {
      ...base,
      colors: {
        ...base.colors,
        background: colors.background,
        card: colors.background,
        text: colors.label,
        border: colors.separator,
        primary: colors.accent,
      },
    };
  }, [dark, theme]);

  const screenOptions = useMemo(() => {
    const { colors, typography } = theme;
    return {
      headerTitle: '',
      headerShadowVisible: false,
      headerTintColor: colors.label,
      headerStyle: { backgroundColor: colors.background },
      drawerStyle: { backgroundColor: colors.background },
      drawerActiveTintColor: colors.accent,
      drawerInactiveTintColor: colors.secondaryLabel,
      drawerActiveBackgroundColor: colors.accentSoft,
      drawerLabelStyle: typography.body,
    };
  }, [theme]);

  const renderDrawerContent = useCallback(
    (props: DrawerContentComponentProps) => (
      <DrawerContent
        {...props}
        themePreference={themePreference}
        onThemePreferenceChange={handleThemePreferenceChange}
      />
    ),
    [themePreference, handleThemePreferenceChange]
  );

  return (
    <ThemeProvider theme={theme}>
      <StatusBar barStyle={dark ? 'light-content' : 'dark-content'} />
      <QueryClientProvider client={queryClient}>
        <SafeAreaProvider>
          <NavigationContainer theme={navTheme}>
            <Drawer.Navigator
              initialRouteName={LAUNCH_ROUTE}
              screenOptions={screenOptions}
              drawerContent={renderDrawerContent}
            >
              <Drawer.Screen
                name="Feed"
                component={FeedScreen}
                options={{ title: 'Feed' }}
              />
              <Drawer.Screen
                name="FeedNative"
                component={FeedNativeScreen}
                options={{ title: 'Feed (Native)' }}
              />
              <Drawer.Screen
                name="Chat"
                component={ChatScreen}
                options={{ title: 'Chat', headerTitle: 'Crew Chat' }}
              />
              <Drawer.Screen
                name="ChatNative"
                component={ChatNativeScreen}
                options={{ title: 'Chat (Native)', headerTitle: 'Crew Chat' }}
              />
              <Drawer.Screen
                name="Assistant"
                component={AssistantScreen}
                options={{ title: 'Assistant', headerTitle: 'Skyfy Assistant' }}
              />
              <Drawer.Screen
                name="Activity"
                component={ActivityScreen}
                options={{ title: 'Activity' }}
              />
              <Drawer.Screen
                name="Nested"
                component={NestedScreen}
                options={{ title: 'Explore' }}
              />
              <Drawer.Screen
                name="NestedNative"
                component={NestedNativeScreen}
                options={{ title: 'Explore (Native)' }}
              />
              <Drawer.Screen
                name="Masonry"
                component={MasonryScreen}
                options={{ title: 'Gallery' }}
              />
              <Drawer.Screen
                name="Contacts"
                component={ContactsScreen}
                options={{ title: 'Companions' }}
              />
              <Drawer.Screen
                name="SectionList"
                component={SectionListScreen}
                options={{ title: 'Directory' }}
              />
              <Drawer.Screen
                name="Reorder"
                component={ReorderScreen}
                options={{ title: 'Boarding Order' }}
              />
              <Drawer.Screen
                name="Tree"
                component={TreeScreen}
                options={{ title: 'Trip Files' }}
              />
              <Drawer.Screen
                name="Snap"
                component={SnapScreen}
                options={{ title: 'Destinations' }}
              />
            </Drawer.Navigator>
          </NavigationContainer>
        </SafeAreaProvider>
      </QueryClientProvider>
    </ThemeProvider>
  );
}
