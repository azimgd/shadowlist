import type { Theme } from '../theme';

/*
 * Match the keyboard to the theme, or the light keyboard flashes white under a dark composer.
 *
 * Only a hex background can be judged here: the macOS theme's colors are AppKit semantic
 * names that resolve inside the renderer. Their brightness is unknowable from JavaScript. That
 * costs nothing on macOS, which has no on-screen keyboard to tint.
 */
export function keyboardAppearanceFor(
  theme: Theme
): 'default' | 'light' | 'dark' {
  const { background } = theme.colors;
  if (typeof background !== 'string') {
    return 'default';
  }
  const hex = /^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/i.exec(background);
  if (!hex) return 'default';
  const [r, g, b] = hex.slice(1).map((channel) => parseInt(channel, 16));
  return 0.299 * r! + 0.587 * g! + 0.114 * b! < 128 ? 'dark' : 'light';
}
