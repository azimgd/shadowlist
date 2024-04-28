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
  props: NativeProps & {
    data: any[];
    renderItem: (payload: { item: any; index: number }) => React.ReactElement;
  },
  forwardedRef: React.Ref<Partial<NativeCommands>>
) => {
  const instanceRef = React.useRef<CraigsListContainerInstance>(null);

  React.useImperativeHandle(forwardedRef, () => ({
    scrollToIndex: (index: number) => {
      Commands.scrollToIndex(instanceRef.current as never, index);
    },
  }));

  return (
    <CraigsListContainerNativeComponent {...props} ref={instanceRef}>
      {props.data.map((item, index) => (
        <CraigsListItemNativeComponent key={index}>
          {props.renderItem({ item, index })}
        </CraigsListItemNativeComponent>
      ))}
    </CraigsListContainerNativeComponent>
  );
};

export default React.forwardRef(CraigsListContainerWrapper);
