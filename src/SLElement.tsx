import React from 'react';
import SLElementNativeComponent, {
  type SLElementNativeProps,
} from './SLElementNativeComponent';

export type SLElementWrapperProps = {};

export type SLElementInstance = InstanceType<typeof SLElementNativeComponent>;

export const SLElement = (
  props: SLElementNativeProps & SLElementWrapperProps
) => {
  const instanceRef = React.useRef<SLElementInstance | null>(null);

  return (
    <SLElementNativeComponent {...props} ref={instanceRef}>
      {props.children}
    </SLElementNativeComponent>
  );
};
