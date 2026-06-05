import { useEffect, useRef, type ReactNode } from 'react';
import { View, Keyboard, Platform, type ViewProps } from 'react-native';

export interface KeyboardDismissViewProps extends ViewProps {
  children?: ReactNode;
  /*
   * When false the wrapper never intercepts touches (fully transparent to gestures).
   * Default true.
   */
  enabled?: boolean;
}

/*
 * Wrapper that dismisses the keyboard when its content is touched. Zero-dependency
 * (RN's Keyboard + the responder system).
 *
 * It only claims a touch *while the keyboard is open*, so when the keyboard is closed
 * it is completely transparent - nested scroll views, buttons and inputs behave
 * exactly as if it were a plain View. While the keyboard is open, the first touch on
 * an otherwise inert area is claimed and dismisses the keyboard on release (the
 * familiar "tap the messages to dismiss" behaviour). Interactive descendants that
 * claim their own touches first (TextInput, Touchables, scrollables) win the responder
 * negotiation, so tapping them does not dismiss - keep the composer outside this
 * wrapper if you want its bar taps to dismiss too.
 */
export function KeyboardDismissView({
  enabled = true,
  children,
  ...viewProps
}: KeyboardDismissViewProps) {
  // Tracked in a ref so the responder callbacks read the latest value without
  // re-rendering on every keyboard toggle.
  const keyboardVisible = useRef(false);

  useEffect(() => {
    const isIos = Platform.OS === 'ios';
    const showEvent = isIos ? 'keyboardWillShow' : 'keyboardDidShow';
    const hideEvent = isIos ? 'keyboardWillHide' : 'keyboardDidHide';

    const showSub = Keyboard.addListener(showEvent, () => {
      keyboardVisible.current = true;
    });
    const hideSub = Keyboard.addListener(hideEvent, () => {
      keyboardVisible.current = false;
    });
    return () => {
      showSub.remove();
      hideSub.remove();
    };
  }, []);

  return (
    <View
      {...viewProps}
      onStartShouldSetResponder={() => enabled && keyboardVisible.current}
      onResponderRelease={() => Keyboard.dismiss()}
      onResponderTerminationRequest={() => true}
    >
      {children}
    </View>
  );
}
