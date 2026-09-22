import { useEffect, useRef, type ReactNode } from 'react';
import { View, Keyboard, Platform, type ViewProps } from 'react-native';

export interface KeyboardViewProps extends ViewProps {
  children?: ReactNode;
  enabled?: boolean;
}

/*
 * Dismisses the keyboard when you tap an empty area. It only takes touches while the
 * keyboard is open, and children that handle their own touches don't dismiss it.
 */
export function KeyboardView({
  enabled = true,
  children,
  ...viewProps
}: KeyboardViewProps) {
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
