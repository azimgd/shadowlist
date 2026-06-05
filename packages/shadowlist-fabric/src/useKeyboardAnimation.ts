import { useEffect, useRef } from 'react';
import { Animated } from 'react-native';
import ShadowlistKeyboard, {
  type KeyboardMoveEvent,
} from './NativeShadowlistKeyboard';

export interface KeyboardAnimation {
  /*
   * Live keyboard height in dp as an Animated.Value, updated every frame of the
   * transition (including interactive drags). Drive a composer/footer with
   * `transform: [{ translateY: Animated.multiply(height, -1) }]`, or a spacer with
   * `height`.
   *
   * Note: this value is updated from JS (setValue per frame), so styles consuming it
   * must NOT use the native driver. For one transform this is smooth; the native
   * driver path would need the Fabric-view-event variant.
   */
  height: Animated.Value;
  /*
   * Transition progress 0..1 as an Animated.Value. Useful for cross-fading or
   * interpolating other properties alongside the keyboard.
   */
  progress: Animated.Value;
}

/*
 * Zero-extra-dependency keyboard animation backed by our own native module
 * (ShadowlistKeyboard), which reads the real keyboard frame natively each frame.
 * Returns Animated.Values that track the keyboard continuously - the basis for
 * frame-accurate, interactive (drag-to-dismiss) keyboard avoidance.
 *
 * The native observer is reference-counted: it starts when the first consumer mounts
 * and stops when the last unmounts.
 */
export function useKeyboardAnimation(): KeyboardAnimation {
  const height = useRef(new Animated.Value(0)).current;
  const progress = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    // Native module absent (not built / unsupported platform): leave the values at 0.
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
