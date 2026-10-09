import { useCallback, useRef } from 'react';
import {
  Keyboard,
  Platform,
  TextInput,
  type GestureResponderEvent,
} from 'react-native';
import type { ScrollEvent } from '../types';

type ScrollBeginDragHandler = (event: ScrollEvent) => void;

interface UseKeyboardDismissResponderOptions {
  keyboardDismissMode: 'none' | 'on-drag' | 'interactive';
  keyboardShouldPersistTaps:
    | boolean
    | 'always'
    | 'never'
    | 'handled'
    | undefined;
  onScrollBeginDrag: ScrollBeginDragHandler | undefined;
  recordInteraction: () => void;
}

interface UseKeyboardDismissResponderResult {
  handleScrollBeginDrag: ScrollBeginDragHandler;
  onStartShouldSetResponderCapture:
    | ((event: GestureResponderEvent) => boolean)
    | undefined;
  onStartShouldSetResponder:
    | ((event: GestureResponderEvent) => boolean)
    | undefined;
  onResponderRelease: (() => void) | undefined;
}

function keyboardIsDismissible() {
  return (
    TextInput.State.currentlyFocusedInput() != null && Keyboard.isVisible()
  );
}

/*
 * A drag counts as an interaction for viewability, and on Android it dismisses the
 * keyboard for keyboardDismissMode, like ScrollView. iOS dismisses it natively.
 * keyboardShouldPersistTaps works like ScrollView. never: a tap while the keyboard is up only
 * dismisses it. handled: a tap no row handles dismisses it. always, or unset: taps reach
 * the rows and the keyboard stays.
 */
export function useKeyboardDismissResponder({
  keyboardDismissMode,
  keyboardShouldPersistTaps,
  onScrollBeginDrag,
  recordInteraction,
}: UseKeyboardDismissResponderOptions): UseKeyboardDismissResponderResult {
  const scrollBeginDragRef = useRef({ onScrollBeginDrag, keyboardDismissMode });
  scrollBeginDragRef.current = { onScrollBeginDrag, keyboardDismissMode };
  const handleScrollBeginDrag = useCallback(
    (event: ScrollEvent) => {
      recordInteraction();
      const current = scrollBeginDragRef.current;
      if (Platform.OS === 'android' && current.keyboardDismissMode !== 'none') {
        Keyboard.dismiss();
      }
      current.onScrollBeginDrag?.(event);
    },
    [recordInteraction]
  );

  const persistTaps =
    keyboardShouldPersistTaps === true ? 'always' : keyboardShouldPersistTaps;
  const handleStartShouldSetResponderCapture = useCallback(
    (event: GestureResponderEvent) =>
      (persistTaps === 'never' || persistTaps === false) &&
      keyboardIsDismissible() &&
      (event.target as unknown) !==
        (TextInput.State.currentlyFocusedInput() as unknown),
    [persistTaps]
  );
  const handleStartShouldSetResponder = useCallback(
    (event: GestureResponderEvent) =>
      persistTaps === 'handled' &&
      keyboardIsDismissible() &&
      (event.target as unknown) !==
        (TextInput.State.currentlyFocusedInput() as unknown),
    [persistTaps]
  );
  const handleResponderRelease = useCallback(() => {
    const input = TextInput.State.currentlyFocusedInput();
    if (input != null) TextInput.State.blurTextInput(input);
  }, []);
  const managesTaps =
    persistTaps === 'never' ||
    persistTaps === false ||
    persistTaps === 'handled';

  return {
    handleScrollBeginDrag,
    onStartShouldSetResponderCapture: managesTaps
      ? handleStartShouldSetResponderCapture
      : undefined,
    onStartShouldSetResponder: managesTaps
      ? handleStartShouldSetResponder
      : undefined,
    onResponderRelease: managesTaps ? handleResponderRelease : undefined,
  };
}
