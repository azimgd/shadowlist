import { useEffect, useRef, type ReactNode } from 'react';
import { View, Keyboard, Platform, type ViewProps } from 'react-native';

export interface KeyboardViewProps extends ViewProps {
  children?: ReactNode;
  enabled?: boolean;
}

/*
 * Wrapper that dismisses the keyboard when an inert area is tapped. It only claims a
 * touch while the keyboard is open, so when closed it is fully transparent to gestures.
 * Interactive descendants that claim their own touches first do not trigger a dismiss.
 */
export function KeyboardView({
  enabled = true,
  children,
  ...viewProps
}: KeyboardViewProps) {
  // Ref so the responder callbacks read the latest value without re-rendering.
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
