import { Platform, type PressableAndroidRippleConfig } from 'react-native';
import type { Theme } from '../theme';

const cache = new WeakMap<Theme, PressableAndroidRippleConfig | null>();

/*
 * The android_ripple of a control under this theme, or undefined when the control shows its
 * pressed style instead. One config per theme object keeps memoized controls from seeing a
 * new prop on every render.
 */
export function pressRippleFor(
  theme: Theme
): PressableAndroidRippleConfig | undefined {
  let ripple = cache.get(theme);
  if (ripple === undefined) {
    ripple =
      theme.pressFeedback === 'ripple' && Platform.OS === 'android'
        ? { color: theme.colors.fill }
        : null;
    cache.set(theme, ripple);
  }
  return ripple ?? undefined;
}
