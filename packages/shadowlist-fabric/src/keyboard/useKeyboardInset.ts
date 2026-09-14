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

  /*
   * measureInWindow resolves asynchronously across the bridge, so a later, faster event
   * (a synchronous hide, or a subsequent show's own measurement) can resolve before an
   * earlier one. Tag each request with an incrementing id and only apply a callback's
   * result if it's still the latest request, so a stale measurement can't overwrite a
   * newer (correct) inset.
   */
  const requestIdRef = useRef(0);

  const commit = useCallback((next: number) => {
    const clamped = next > 0 ? next : 0;
    setInset((prev) => (prev === clamped ? prev : clamped));
  }, []);

  const handleShow = useCallback(
    (event: KeyboardEvent) => {
      const requestId = ++requestIdRef.current;
      const screenHeight = Dimensions.get('window').height;

      const keyboardTopY =
        event.endCoordinates.screenY ??
        screenHeight - event.endCoordinates.height;

      const node = viewRef.current;
      if (node?.measureInWindow) {
        node.measureInWindow((_x, y, _width, height) => {
          if (requestIdRef.current !== requestId) return;
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
    const hideSub = Keyboard.addListener(hideEvent, () => {
      /*
       * Invalidate any in-flight measureInWindow from a prior show so it can't land
       * after this (synchronous) hide and re-open the inset.
       */
      requestIdRef.current++;
      commit(0);
    });

    return () => {
      showSub.remove();
      hideSub.remove();
    };
  }, [enabled, handleShow, commit]);

  return enabled ? inset : 0;
}
