import React from 'react';
import SLElementNativeComponent, {
  type SLElementNativeProps,
} from './SLElementNativeComponent';

export type SLElementWrapperProps = {};

export type SLElementInstance = InstanceType<typeof SLElementNativeComponent>;

const SLElementWrapper = (
  props: SLElementNativeProps & SLElementWrapperProps,
  forwardedRef: React.Ref<{}>
) => {
  const instanceRef = React.useRef<SLElementInstance | null>(null);

  React.useImperativeHandle(forwardedRef, () => ({}));

  return (
    <SLElementNativeComponent {...props} ref={instanceRef}>
      {props.children}
    </SLElementNativeComponent>
  );
};

export const SLElement = React.forwardRef(SLElementWrapper);
