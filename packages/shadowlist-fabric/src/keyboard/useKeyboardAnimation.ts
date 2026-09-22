import { useEffect, useRef } from 'react';
import { Animated } from 'react-native';
import ShadowListKeyboard, {
  type KeyboardMoveEvent,
} from './NativeShadowListKeyboard';

export interface KeyboardAnimation {
  height: Animated.Value;
  progress: Animated.Value;
}

/*
 * The native module is one global switch. Count mounted hooks so one screen going away
 * doesn't stop keyboard events for another that is still mounted.
 */
let enabledCount = 0;

export function useKeyboardAnimation(): KeyboardAnimation {
  const height = useRef(new Animated.Value(0)).current;
  const progress = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    if (!ShadowListKeyboard) {
      return;
    }

    if (enabledCount++ === 0) ShadowListKeyboard.setEnabled(true);
    const subscription = ShadowListKeyboard.onKeyboardMove(
      (event: KeyboardMoveEvent) => {
        height.setValue(event.height);
        progress.setValue(event.progress);
      }
    );

    return () => {
      subscription.remove();
      if (--enabledCount === 0) ShadowListKeyboard?.setEnabled(false);
    };
  }, [height, progress]);

  return { height, progress };
}
