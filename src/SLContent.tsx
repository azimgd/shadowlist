import React from 'react';
import SLContentNativeComponent, {
  type SLContentNativeProps,
} from './SLContentNativeComponent';

export type SLContentWrapperProps = {};

export type SLContentInstance = InstanceType<typeof SLContentNativeComponent>;

export const SLContent = (
  props: SLContentNativeProps & SLContentWrapperProps
) => {
  const instanceRef = React.useRef<SLContentInstance | null>(null);

  return (
    <SLContentNativeComponent {...props} ref={instanceRef}>
      {props.children}
    </SLContentNativeComponent>
  );
};
