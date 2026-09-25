import { GestureHandlerRootView } from 'react-native-gesture-handler';
import { useMemo } from 'react';
import { enableScreens } from 'react-native-screens';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import {
  NavigationContainer,
  DarkTheme,
  DefaultTheme,
  type InitialState,
} from '@react-navigation/native';
import {
  createNativeStackNavigator,
  type NativeStackNavigationOptions,
} from '@react-navigation/native-stack';
import { ThemeProvider } from 'shadowlist-utils/native';
import { QueryClientProvider } from '@tanstack/react-query';
import { queryClient } from './queries/queryClient';
import { Appearance, Platform, StatusBar, StyleSheet } from 'react-native';
import { network } from './api/network';
import { launchSetting } from './launchSettings';
import './jsFrameMonitor';
import { groupedColors, useAppTheme } from './appTheme';
import { HomeScreen } from './HomeScreen';
import { ContactDetailScreen } from './ContactDetailScreen';
import { EXAMPLES, type RootStackParamList } from './routes';

enableScreens();

const launchLatency = launchSetting('SLLatency')?.split(',').map(Number);
if (launchLatency?.length === 2 && launchLatency.every(Number.isFinite)) {
  network.latencyMs = { min: launchLatency[0]!, max: launchLatency[1]! };
}
const launchFailureRate = Number(launchSetting('SLSendFailureRate'));
if (launchFailureRate >= 0 && launchFailureRate <= 1) {
  network.sendFailureRate = launchFailureRate;
}

const launchTheme = launchSetting('SLTheme');
if (launchTheme === 'light' || launchTheme === 'dark') {
  Appearance.setColorScheme(launchTheme);
}

const launchRoute = EXAMPLES.find(
  (example) => example.route === launchSetting('SLRoute')
)?.route;
const INITIAL_STATE: InitialState | undefined = launchRoute
  ? { routes: [{ name: 'Home' }, { name: launchRoute }] }
  : undefined;

const Stack = createNativeStackNavigator<RootStackParamList>();

export default function App() {
  const { theme, dark } = useAppTheme();

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

  const screenOptions = useMemo<NativeStackNavigationOptions>(() => {
    const { colors } = theme;
    return Platform.OS === 'ios'
      ? {
          headerTintColor: colors.accent,
          headerTitleStyle: { color: colors.label },
          headerLargeTitleStyle: { color: colors.label },
          headerLargeTitleShadowVisible: false,
          headerBackButtonDisplayMode: 'minimal',
        }
      : {
          headerTintColor: colors.accent,
          headerTitleAlign: 'center',
          headerTitleStyle: { color: colors.label },
          headerStyle: { backgroundColor: colors.background },
          headerShadowVisible: false,
          animation: 'slide_from_right',
        };
  }, [theme]);

  /*
   * ShadowList manages its own insets, so list screens can't sit under a see-through bar.
   */
  const exampleOptions = useMemo<NativeStackNavigationOptions>(
    () => ({
      headerStyle: { backgroundColor: theme.colors.background },
      headerShadowVisible: true,
    }),
    [theme]
  );

  return (
    <GestureHandlerRootView style={styles.root}>
      <ThemeProvider theme={theme}>
        <StatusBar
          barStyle={dark ? 'light-content' : 'dark-content'}
          backgroundColor={theme.colors.background}
        />
        <QueryClientProvider client={queryClient}>
          <SafeAreaProvider>
            <NavigationContainer theme={navTheme} initialState={INITIAL_STATE}>
              <Stack.Navigator screenOptions={screenOptions}>
                <Stack.Screen
                  name="Home"
                  component={HomeScreen}
                  options={{
                    title: 'ShadowList',
                    headerLargeTitle: true,
                    headerStyle:
                      Platform.OS === 'android'
                        ? { backgroundColor: groupedColors(theme).page }
                        : undefined,
                  }}
                />
                {EXAMPLES.map((example) => (
                  <Stack.Screen
                    key={example.route}
                    name={example.route}
                    component={example.component}
                    options={{ ...exampleOptions, title: example.title }}
                  />
                ))}
                <Stack.Screen
                  name="ContactDetail"
                  component={ContactDetailScreen}
                  options={{ title: '' }}
                />
              </Stack.Navigator>
            </NavigationContainer>
          </SafeAreaProvider>
        </QueryClientProvider>
      </ThemeProvider>
    </GestureHandlerRootView>
  );
}

const styles = StyleSheet.create({
  root: {
    flex: 1,
  },
});
