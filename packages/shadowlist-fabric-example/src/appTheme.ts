import { useEffect, useState } from 'react';
import { AccessibilityInfo, Platform, useColorScheme } from 'react-native';
import {
  createTheme,
  darkTheme,
  lightTheme,
  type Theme,
} from 'shadowlist-utils/native';

/*
 * Increase Contrast variants, using Apple's accessible system colors.
 */
const lightHighContrast = createTheme(lightTheme, {
  colors: {
    secondaryLabel: 'rgba(60,60,67,0.85)',
    tertiaryLabel: 'rgba(60,60,67,0.6)',
    separator: 'rgba(60,60,67,0.5)',
    accent: '#0040DD',
    accentSoft: 'rgba(0,64,221,0.16)',
    blue: '#0040DD',
    green: '#248A3D',
    red: '#D70015',
    redSoft: 'rgba(215,0,21,0.16)',
  },
});

const darkHighContrast = createTheme(darkTheme, {
  colors: {
    secondaryLabel: 'rgba(235,235,245,0.85)',
    tertiaryLabel: 'rgba(235,235,245,0.6)',
    separator: 'rgba(120,120,128,0.8)',
    accent: '#409CFF',
    accentSoft: 'rgba(64,156,255,0.2)',
    blue: '#409CFF',
    green: '#30DB5B',
    red: '#FF6961',
    redSoft: 'rgba(255,105,97,0.2)',
  },
});

const contrastEvent =
  Platform.OS === 'ios'
    ? 'darkerSystemColorsChanged'
    : 'highTextContrastChanged';

function useIncreasedContrast(): boolean {
  const [enabled, setEnabled] = useState(false);
  useEffect(() => {
    const read =
      Platform.OS === 'ios'
        ? AccessibilityInfo.isDarkerSystemColorsEnabled()
        : AccessibilityInfo.isHighTextContrastEnabled();
    read.then(setEnabled, () => {});
    const subscription = AccessibilityInfo.addEventListener(
      contrastEvent,
      setEnabled
    );
    return () => subscription.remove();
  }, []);
  return enabled;
}

export function useAppTheme(): { theme: Theme; dark: boolean } {
  const dark = useColorScheme() === 'dark';
  const highContrast = useIncreasedContrast();
  const theme = dark
    ? highContrast
      ? darkHighContrast
      : darkTheme
    : highContrast
      ? lightHighContrast
      : lightTheme;
  return { theme, dark };
}

export const GROUP_RADIUS =
  Platform.OS === 'ios' && parseInt(String(Platform.Version), 10) < 26
    ? 10
    : 26;

export function groupedColors(theme: Theme) {
  const dark = theme.colors.background === darkTheme.colors.background;
  return dark
    ? { page: theme.colors.background, cell: theme.colors.elevated }
    : { page: theme.colors.elevated, cell: theme.colors.background };
}
