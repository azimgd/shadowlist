import { useCallback, useEffect, useRef, useState } from 'react';
import {
  Dimensions,
  Keyboard,
  Platform,
  type KeyboardEvent,
} from 'react-native';

export interface MeasurableRef {
  measureInWindow?: (
    callback: (x: number, y: number, width: number, height: number) => void
  ) => void;
}

export interface UseKeyboardInsetOptions {
  enabled?: boolean;
  offset?: number;
}

/*
 * Returns the bottom inset (px) the list should reserve for the keyboard: the
 * overlap between the keyboard frame and the list's on-screen frame, never negative.
 * Feed the result into ShadowList's contentInsetBottom prop.
 */
export function useKeyboardInset(
  viewRef: { current: MeasurableRef | null },
  { enabled = false, offset = 0 }: UseKeyboardInsetOptions = {}
): number {
  const [inset, setInset] = useState(0);

  const offsetRef = useRef(offset);
  offsetRef.current = offset;

  const commit = useCallback((next: number) => {
    const clamped = next > 0 ? next : 0;
    setInset((prev) => (prev === clamped ? prev : clamped));
  }, []);

  const handleShow = useCallback(
    (event: KeyboardEvent) => {
      const screenHeight = Dimensions.get('window').height;

      const keyboardTopY =
        event.endCoordinates.screenY ??
        screenHeight - event.endCoordinates.height;

      const node = viewRef.current;
      if (node?.measureInWindow) {
        node.measureInWindow((_x, y, _width, height) => {
          commit(y + height - keyboardTopY - offsetRef.current);
        });
      } else {
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
