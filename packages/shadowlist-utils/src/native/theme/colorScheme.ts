import { Appearance, type ColorSchemeName } from 'react-native';

// One Appearance listener shared by every useTheme without a provider, instead of one per component.
const listeners = new Set<() => void>();
let subscription: { remove: () => void } | undefined;

export function subscribeColorScheme(listener: () => void): () => void {
  listeners.add(listener);
  if (subscription === undefined) {
    subscription = Appearance.addChangeListener(() => {
      listeners.forEach((notify) => notify());
    });
  }
  return () => {
    listeners.delete(listener);
    if (listeners.size === 0 && subscription !== undefined) {
      subscription.remove();
      subscription = undefined;
    }
  };
}

export function getColorScheme(): ColorSchemeName | null | undefined {
  return Appearance.getColorScheme();
}
