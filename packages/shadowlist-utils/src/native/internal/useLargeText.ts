import { useSyncExternalStore } from 'react';
import { Dimensions } from 'react-native';

const LARGE_TEXT_SCALE = 1.3;

function readLargeText(): boolean {
  return Dimensions.get('window').fontScale >= LARGE_TEXT_SCALE;
}

/*
 * One Dimensions listener shared by every row, instead of one per row through
 * useWindowDimensions. Rows also re-render only when the flag flips, not on every window
 * change like a rotation.
 */
const listeners = new Set<() => void>();
let subscription: { remove: () => void } | null = null;
let largeText = false;

function subscribe(listener: () => void): () => void {
  listeners.add(listener);
  if (subscription === null) {
    largeText = readLargeText();
    subscription = Dimensions.addEventListener(
      'change',
      ({ window }: { window: { fontScale: number } }) => {
        const next = window.fontScale >= LARGE_TEXT_SCALE;
        if (next === largeText) return;
        largeText = next;
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

function getSnapshot(): boolean {
  return subscription === null ? readLargeText() : largeText;
}

/*
 * True at accessibility text sizes, where rows wrap instead of truncating.
 */
export function useLargeText(): boolean {
  return useSyncExternalStore(subscribe, getSnapshot);
}
