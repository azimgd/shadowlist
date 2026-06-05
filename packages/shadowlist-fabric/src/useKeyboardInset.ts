import { useCallback, useEffect, useRef, useState } from 'react';
import {
  Dimensions,
  Keyboard,
  Platform,
  type KeyboardEvent,
} from 'react-native';

/*
 * Minimal viewport handle: anything with measureInWindow (a native host ref). The
 * hook measures the list's on-screen frame when the keyboard appears so the inset is
 * the actual overlap with the list, not the raw keyboard height - a list that does
 * not reach the bottom of the screen (tab bar, padding) is not over-inset.
 */
export interface MeasurableRef {
  measureInWindow?: (
    callback: (x: number, y: number, width: number, height: number) => void
  ) => void;
}

export interface UseKeyboardInsetOptions {
  /*
   * When false the hook is inert and always returns 0 (the keyboard listeners are
   * not even attached), so it costs nothing when keyboard avoidance is off.
   */
  enabled?: boolean;
  /*
   * Extra pixels subtracted from the computed overlap. Use it to discount UI that
   * already sits above the keyboard (a bottom tab bar) or safe-area inset already
   * applied below the list. Defaults to 0.
   */
  offset?: number;
}

/*
 * Zero-dependency keyboard avoidance. Subscribes to React Native's built-in Keyboard
 * events and returns the bottom inset (px) the list should reserve for the keyboard -
 * the overlap between the keyboard frame and the list's on-screen frame, never
 * negative. Feed the result into Shadowlist's contentInsetBottom prop; the native
 * side grows the scroll inset and slides the content up so the rows behind the
 * keyboard come into view.
 *
 * iOS uses the will-show/hide events (they fire before the animation, so the inset
 * change rides the same frame); Android only reliably emits did-show/hide. Either
 * way the actual smoothing is done natively.
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
      // screenY is the keyboard's top edge in screen coords (iOS, and most Android
      // implementations); fall back to deriving it from the height when absent.
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
