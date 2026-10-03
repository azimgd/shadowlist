/*
 * A macOS window has no safe area, and react-native-safe-area-context has no macOS build.
 */
const NO_INSETS = { top: 0, right: 0, bottom: 0, left: 0 };

export function useSafeAreaInsets(): typeof NO_INSETS {
  return NO_INSETS;
}
