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
  const [_registered, setRegistered] = React.useState(false);
  const instanceRef = React.useRef<SLElementInstance | null>(null);

  React.useImperativeHandle(forwardedRef, () => ({}));

  const nextRef = (ref: SLElementInstance) => {
    if (ref) {
      global.__NATIVE_registerElementNode(
        ReactNativeInterface.getNodeFromPublicInstance(ref)
      );
      instanceRef.current = ref;
      setRegistered(true);
    } else {
      global.__NATIVE_unregisterElementNode(
        ReactNativeInterface.getNodeFromPublicInstance(instanceRef.current)
      );
      instanceRef.current = null;
      setRegistered(false);
    }

    if (forwardedRef) {
      if (typeof forwardedRef === 'function') {
        // @ts-ignore
        forwardedRef(ref);
      } else {
        (forwardedRef as React.MutableRefObject<any>).current = ref;
      }
    }
  };

  return (
    <SLElementNativeComponent {...props} ref={nextRef}>
      {props.children}
    </SLElementNativeComponent>
  );
};

export const SLElement = React.forwardRef(SLElementWrapper);
