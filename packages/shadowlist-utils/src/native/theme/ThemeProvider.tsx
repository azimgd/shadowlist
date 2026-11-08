import type { ReactNode } from 'react';
import type { Theme } from './Theme';
import { ThemeContext } from './ThemeContext';

export interface ThemeProviderProps {
  theme: Theme;
  children?: ReactNode;
}

export function ThemeProvider({ theme, children }: ThemeProviderProps) {
  return (
    <ThemeContext.Provider value={theme}>{children}</ThemeContext.Provider>
  );
}
