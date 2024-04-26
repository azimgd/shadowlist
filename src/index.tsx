import React from 'react';
import CraigsListContainerNativeComponent, {
  Commands,
  type NativeProps,
  type NativeCommands,
} from './CraigsListContainerNativeComponent';
import CraigsListItemNativeComponent from './CraigsListItemNativeComponent';

export type CraigsListContainerInstance = InstanceType<
  typeof CraigsListContainerNativeComponent
>;

const CraigsListContainerWrapper = (
  props: NativeProps,
  forwardedRef: React.Ref<Partial<NativeCommands>>
) => {
  const instanceRef = React.useRef<CraigsListContainerInstance>(null);

  React.useImperativeHandle(forwardedRef, () => ({
    scrollToIndex: (index: number) => {
      Commands.scrollToIndex(instanceRef.current as never, index);
    },
  }));

  return <CraigsListContainerNativeComponent {...props} ref={instanceRef} />;
};

export const CraigsListContainer = React.forwardRef(CraigsListContainerWrapper);
export const CraigsListItem = CraigsListItemNativeComponent;
