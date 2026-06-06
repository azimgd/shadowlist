import { useCallback, useEffect, useRef, useState } from 'react';
import {
  Dimensions,
  Keyboard,
  Platform,
  type KeyboardEvent,
} from 'react-native';

/*
 * Minimal viewport handle: anything with measureInWindow. Measuring the list's frame
 * lets the inset be the actual keyboard overlap, not the raw keyboard height.
 */
export interface MeasurableRef {
  measureInWindow?: (
    callback: (x: number, y: number, width: number, height: number) => void
  ) => void;
}

export interface UseKeyboardInsetOptions {
  /*
   * When false the hook is inert and returns 0 (listeners are not attached).
   */
  enabled?: boolean;
  /*
   * Pixels subtracted from the computed overlap, to discount UI already above the
   * keyboard (a tab bar) or safe-area inset below the list. Defaults to 0.
   */
  offset?: number;
}

/*
 * Returns the bottom inset (px) the list should reserve for the keyboard: the
 * overlap between the keyboard frame and the list's on-screen frame, never negative.
 * Feed the result into Shadowlist's contentInsetBottom prop.
 */
export function useKeyboardInset(
  viewRef: { current: MeasurableRef | null },
  { enabled = false, offset = 0 }: UseKeyboardInsetOptions = {}
): number {
  const [inset, setInset] = useState(0);

  // Read offset through a ref so changing it does not re-subscribe the listeners.
  const offsetRef = useRef(offset);
  offsetRef.current = offset;

  const commit = useCallback((next: number) => {
    const clamped = next > 0 ? next : 0;
    setInset((prev) => (prev === clamped ? prev : clamped));
  }, []);

  const handleShow = useCallback(
    (event: KeyboardEvent) => {
      const screenHeight = Dimensions.get('window').height;
      // Keyboard's top edge in screen coords; derive from height when absent.
      const keyboardTopY =
        event.endCoordinates.screenY ??
        screenHeight - event.endCoordinates.height;

      const node = viewRef.current;
      if (node?.measureInWindow) {
        node.measureInWindow((_x, y, _width, height) => {
          commit(y + height - keyboardTopY - offsetRef.current);
        });
      } else {
        // No node to measure: assume the list reaches the bottom of the screen.
        commit(screenHeight - keyboardTopY - offsetRef.current);
      }
    },
    [commit, viewRef]
  );

  useEffect(() => {
    if (!enabled) {
      setInset((prev) => (prev === 0 ? prev : 0));
      return;
    }

    const isIos = Platform.OS === 'ios';
    const showEvent = isIos ? 'keyboardWillShow' : 'keyboardDidShow';
    const hideEvent = isIos ? 'keyboardWillHide' : 'keyboardDidHide';

    const showSub = Keyboard.addListener(showEvent, handleShow);
    const hideSub = Keyboard.addListener(hideEvent, () => commit(0));

    return () => {
      showSub.remove();
      hideSub.remove();
    };
  }, [enabled, handleShow, commit]);

  return enabled ? inset : 0;
}
