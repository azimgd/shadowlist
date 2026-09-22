import { useWindowDimensions } from 'react-native';

const LARGE_TEXT_SCALE = 1.3;

/*
 * True at accessibility text sizes, where rows wrap instead of truncating.
 */
export function useLargeText(): boolean {
  return useWindowDimensions().fontScale >= LARGE_TEXT_SCALE;
}
