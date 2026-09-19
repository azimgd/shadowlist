import type { Theme } from '../theme';

// Matches the keyboard to the theme; the default light keyboard flashes white against a dark composer.
export function keyboardAppearanceFor(
  theme: Theme
): 'default' | 'light' | 'dark' {
  const hex = /^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i.exec(
    theme.colors.background
  );
  if (!hex) return 'default';
  const [r, g, b] = hex.slice(1).map((channel) => parseInt(channel, 16));
  return 0.299 * r! + 0.587 * g! + 0.114 * b! < 128 ? 'dark' : 'light';
}
