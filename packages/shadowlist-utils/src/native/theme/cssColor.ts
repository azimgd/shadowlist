import type { ColorValue } from 'react-native';

/*
 * React Navigation and native-stack take colors as plain CSS strings rather than the
 * `ColorValue` React Native itself accepts. A token that could be an AppKit semantic
 * color has to be narrowed to satisfy them. It is a type-level assertion, not a conversion:
 * a theme built from semantic colors handed to those libraries would render nothing.
 */
export function cssColor(color: ColorValue): string {
  return color as string;
}
