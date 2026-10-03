import { useContext, useSyncExternalStore } from 'react';
import { Platform } from 'react-native';
import type { Theme } from './Theme';
import { ThemeContext } from './ThemeContext';
import { darkTheme } from './darkTheme';
import { lightTheme } from './lightTheme';
import { macOSAppKitTheme } from './macOSTokens';
import { getColorScheme, subscribeColorScheme } from './colorScheme';

const subscribeNothing = () => () => {};
const getNothing = () => undefined;

/*
 * A macOS app runs one theme for both appearances: its colors are AppKit semantic colors,
 * which AppKit resolves per view as the appearance changes. Choosing a light or dark token
 * set here would pin one appearance and leave the list lit when the system goes dark.
 */
export function systemTheme(): Theme {
  if (Platform.OS === 'macos') {
    return macOSAppKitTheme;
  }
  return getColorScheme() === 'dark' ? darkTheme : lightTheme;
}

export function useTheme(): Theme {
  const theme = useContext(ThemeContext);
  // Only a system theme outside macOS has to re-render on an appearance change.
  const followsAppearance = theme === undefined && Platform.OS !== 'macos';
  useSyncExternalStore(
    followsAppearance ? subscribeColorScheme : subscribeNothing,
    followsAppearance ? getColorScheme : getNothing
  );
  return theme ?? systemTheme();
}
