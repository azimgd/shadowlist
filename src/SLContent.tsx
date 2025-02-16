import React from 'react';
import SLContentNativeComponent, {
  type SLContentNativeProps,
} from './SLContentNativeComponent';

// @ts-ignore
// import ReactNativeInterface from 'react-native/Libraries/ReactPrivate/ReactNativePrivateInterface';

export type SLContentWrapperProps = {};

export type SLContentInstance = InstanceType<typeof SLContentNativeComponent>;

const SLContentWrapper = (
  props: SLContentNativeProps & SLContentWrapperProps,
  forwardedRef: React.Ref<{}>
) => {
  const instanceRef = React.useRef<SLContentInstance | null>(null);

  React.useImperativeHandle(forwardedRef, () => ({}));

  const nextRef = (ref: SLContentInstance) => {
    if (ref) {
      // global.__NATIVE_registerElementNode(
      //   ReactNativeInterface.getNodeFromPublicInstance(ref)
      // );
      instanceRef.current = ref;
    } else {
      // global.__NATIVE_unregisterElementNode(
      //   ReactNativeInterface.getNodeFromPublicInstance(instanceRef.current)
      // );
      instanceRef.current = null;
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
    <SLContentNativeComponent {...props} ref={nextRef}>
      {props.children}
    </SLContentNativeComponent>
  );
};

export const SLContent = React.forwardRef(SLContentWrapper);
