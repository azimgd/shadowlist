import { useContext, useSyncExternalStore } from 'react';
import type { Theme } from './Theme';
import { ThemeContext } from './ThemeContext';
import { darkTheme } from './darkTheme';
import { lightTheme } from './lightTheme';
import { getColorScheme, subscribeColorScheme } from './colorScheme';

const subscribeNothing = () => () => {};
const getNothing = () => undefined;

export function useTheme(): Theme {
  const theme = useContext(ThemeContext);
  const colorScheme = useSyncExternalStore(
    theme === undefined ? subscribeColorScheme : subscribeNothing,
    theme === undefined ? getColorScheme : getNothing
  );
  if (theme !== undefined) {
    return theme;
  }
  return colorScheme === 'dark' ? darkTheme : lightTheme;
}
