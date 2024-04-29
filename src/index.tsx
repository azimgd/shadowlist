import React from 'react';
import ShadowListContainerNativeComponent, {
  Commands,
  type NativeProps,
  type NativeCommands,
} from './ShadowListContainerNativeComponent';
import ShadowListItemNativeComponent from './ShadowListItemNativeComponent';

export type ShadowListContainerInstance = InstanceType<
  typeof ShadowListContainerNativeComponent
>;

const ShadowListContainerWrapper = (
  props: NativeProps & {
    data: any[];
    renderItem: (payload: { item: any; index: number }) => React.ReactElement;
  },
  forwardedRef: React.Ref<Partial<NativeCommands>>
) => {
  const instanceRef = React.useRef<ShadowListContainerInstance>(null);

  React.useImperativeHandle(forwardedRef, () => ({
    scrollToIndex: (index: number) => {
      Commands.scrollToIndex(instanceRef.current as never, index);
    },
  }));

  const data = props.inverted ? props.data.reverse() : props.data;

  return (
    <ShadowListContainerNativeComponent {...props} ref={instanceRef}>
      {data.map((item, index) => (
        <ShadowListItemNativeComponent key={index}>
          {props.renderItem({ item, index })}
        </ShadowListItemNativeComponent>
      ))}
    </ShadowListContainerNativeComponent>
  );
};

export default React.forwardRef(ShadowListContainerWrapper);
