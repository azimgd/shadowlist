import React from 'react';
import SLContentNativeComponent, {
  type SLContentNativeProps,
} from './SLContentNativeComponent';

export type SLContentWrapperProps = {};

export type SLContentInstance = InstanceType<typeof SLContentNativeComponent>;

const SLContentWrapper = (
  props: SLContentNativeProps & SLContentWrapperProps,
  forwardedRef: React.Ref<{}>
) => {
  const instanceRef = React.useRef<SLContentInstance | null>(null);

  React.useImperativeHandle(forwardedRef, () => ({}));

  return (
    <SLContentNativeComponent {...props} ref={instanceRef}>
      {props.children}
    </SLContentNativeComponent>
  );
};

export const SLContent = React.forwardRef(SLContentWrapper);
