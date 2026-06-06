import { useEffect, useRef } from 'react';
import { Animated } from 'react-native';
import ShadowlistKeyboard, {
  type KeyboardMoveEvent,
} from './NativeShadowlistKeyboard';

export interface KeyboardAnimation {
  /*
   * Live keyboard height in dp, updated every frame of the transition. Updated from
   * JS per frame, so consuming styles must NOT use the native driver.
   */
  height: Animated.Value;
  /*
   * Transition progress 0..1.
   */
  progress: Animated.Value;
}

/*
 * Returns Animated.Values that track the keyboard continuously. The native observer
 * is reference-counted: starts on first consumer mount, stops on last unmount.
 */
export function useKeyboardAnimation(): KeyboardAnimation {
  const height = useRef(new Animated.Value(0)).current;
  const progress = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    // Native module absent: leave the values at 0.
    if (!ShadowlistKeyboard) {
      return;
    }

    ShadowlistKeyboard.setEnabled(true);
    const subscription = ShadowlistKeyboard.onKeyboardMove(
      (event: KeyboardMoveEvent) => {
        height.setValue(event.height);
        progress.setValue(event.progress);
      }
    );

    return () => {
      subscription.remove();
      ShadowlistKeyboard?.setEnabled(false);
    };
  }, [height, progress]);

  return { height, progress };
}
