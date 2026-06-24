import { useEffect, useRef } from 'react';
import { Animated } from 'react-native';
import ShadowListKeyboard, {
  type KeyboardMoveEvent,
} from './NativeShadowListKeyboard';

export interface KeyboardAnimation {
  height: Animated.Value;
  progress: Animated.Value;
}

export function useKeyboardAnimation(): KeyboardAnimation {
  const height = useRef(new Animated.Value(0)).current;
  const progress = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    if (!ShadowListKeyboard) {
      return;
    }

    ShadowListKeyboard.setEnabled(true);
    const subscription = ShadowListKeyboard.onKeyboardMove(
      (event: KeyboardMoveEvent) => {
        height.setValue(event.height);
        progress.setValue(event.progress);
      }
    );

    return () => {
      subscription.remove();
      ShadowListKeyboard?.setEnabled(false);
    };
  }, [height, progress]);

  return { height, progress };
}
