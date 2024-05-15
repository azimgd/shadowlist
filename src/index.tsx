import React from 'react';
import ShadowListContainerNativeComponent, {
  Commands,
  type NativeProps,
  type NativeCommands,
} from './ShadowListContainerNativeComponent';
import ShadowListItemNativeComponent from './ShadowListItemNativeComponent';

type Component =
  | React.ComponentType<any>
  | React.ReactElement
  | null
  | undefined;

const invoker = (Component: Component) =>
  // @ts-ignore
  React.isValidElement(Component) ? Component : <Component />;

export type JSProps = {
  data: any[];
  renderItem: (payload: { item: any; index: number }) => React.ReactElement;
  ListHeaderComponent?: Component;
  ListFooterComponent?: Component;
  ListEmptyComponent?: Component;
};

export type ShadowListContainerInstance = InstanceType<
  typeof ShadowListContainerNativeComponent
>;

const ShadowListContainerWrapper = (
  props: NativeProps & JSProps,
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
    <ShadowListContainerNativeComponent
      {...props}
      ref={instanceRef}
      hasListHeaderComponent={!!props.ListHeaderComponent}
      hasListFooterComponent={!!props.ListFooterComponent}
    >
      <ShadowListItemNativeComponent key={-1}>
        {invoker(props.ListHeaderComponent)}
      </ShadowListItemNativeComponent>

      {data.length ? (
        data.map((item, index) => (
          <ShadowListItemNativeComponent key={index}>
            {props.renderItem({ item, index })}
          </ShadowListItemNativeComponent>
        ))
      ) : (
        <ShadowListItemNativeComponent>
          {invoker(props.ListEmptyComponent)}
        </ShadowListItemNativeComponent>
      )}

      <ShadowListItemNativeComponent key={data.length}>
        {invoker(props.ListFooterComponent)}
      </ShadowListItemNativeComponent>
    </ShadowListContainerNativeComponent>
  );
};

export default React.forwardRef(ShadowListContainerWrapper);
