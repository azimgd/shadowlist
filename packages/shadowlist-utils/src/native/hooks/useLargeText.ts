import { useCallback, useSyncExternalStore } from 'react';
import { Dimensions } from 'react-native';

const DEFAULT_THRESHOLD = 1.3;

/*
 * One Dimensions listener shared by every caller, instead of one per row through
 * useWindowDimensions. Callers are told only when the font scale changes, not on every window
 * change like a rotation.
 */
const listeners = new Set<() => void>();
let subscription: { remove: () => void } | null = null;
let fontScale = 1;

function subscribe(listener: () => void): () => void {
  listeners.add(listener);
  if (subscription === null) {
    fontScale = Dimensions.get('window').fontScale;
    subscription = Dimensions.addEventListener(
      'change',
      ({ window }: { window: { fontScale: number } }) => {
        if (window.fontScale === fontScale) {
          return;
        }
        fontScale = window.fontScale;
        listeners.forEach((notify) => notify());
      }
    );
  }
  return () => {
    listeners.delete(listener);
    if (listeners.size === 0 && subscription !== null) {
      subscription.remove();
      subscription = null;
    }
  };
}

function readFontScale(): number {
  return subscription === null ? Dimensions.get('window').fontScale : fontScale;
}

/*
 * True at accessibility text sizes, where rows wrap instead of truncating. A caller re-renders
 * only when the result flips.
 */
export function useLargeText(threshold: number = DEFAULT_THRESHOLD): boolean {
  const getSnapshot = useCallback(
    () => readFontScale() >= threshold,
    [threshold]
  );
  return useSyncExternalStore(subscribe, getSnapshot);
}
