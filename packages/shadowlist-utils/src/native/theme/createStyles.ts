import type { Theme } from './Theme';
import { useTheme } from './useTheme';

/*
 * Styles are cached per theme object, so memoized rows never rebuild a stylesheet on render.
 */
export function createStyles<T extends object>(
  factory: (theme: Theme) => T
): () => T {
  const cache = new WeakMap<Theme, T>();
  return function useStyles() {
    const theme = useTheme();
    let styles = cache.get(theme);
    if (styles === undefined) {
      styles = factory(theme);
      cache.set(theme, styles);
    }
    return styles;
  };
}
