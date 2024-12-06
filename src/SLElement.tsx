import React from 'react';
import SLElementNativeComponent, {
  type SLElementNativeProps,
} from './SLElementNativeComponent';

// @ts-ignore
import ReactNativeInterface from 'react-native/Libraries/ReactPrivate/ReactNativePrivateInterface';

export type SLElementWrapperProps = {};

export type SLElementInstance = InstanceType<typeof SLElementNativeComponent>;

const SLElementWrapper = (
  props: SLElementNativeProps & SLElementWrapperProps,
  forwardedRef: React.Ref<{}>
) => {
  const instanceRef = React.useRef<SLElementInstance>(null);

  React.useLayoutEffect(() => {
    // @ts-ignore
    global.__NATIVE_registerElementNode(
      ReactNativeInterface.getNodeFromPublicInstance(instanceRef.current)
    );
  }, []);

  React.useImperativeHandle(forwardedRef, () => ({}));

  return (
    <SLElementNativeComponent {...props} ref={instanceRef}>
      {props.children}
    </SLElementNativeComponent>
  );
};

export const SLElement = React.forwardRef(SLElementWrapper);
