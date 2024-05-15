import React from 'react';
import { type ViewStyle } from 'react-native';
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
  contentContainerStyle?: ViewStyle;
  ListHeaderComponent?: Component;
  ListHeaderComponentStyle?: ViewStyle;
  ListFooterComponent?: Component;
  ListFooterComponentStyle?: ViewStyle;
  ListEmptyComponent?: Component;
  ListEmptyComponentStyle?: ViewStyle;
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
    scrollToOffset: (offset: number) => {
      Commands.scrollToOffset(instanceRef.current as never, offset);
    },
  }));

  const data = props.inverted ? props.data.reverse() : props.data;

  return (
    <ShadowListContainerNativeComponent
      ref={instanceRef}
      hasListHeaderComponent={!!props.ListHeaderComponent}
      hasListFooterComponent={!!props.ListFooterComponent}
      style={props.contentContainerStyle}
    >
      {props.ListHeaderComponent ? (
        <ShadowListItemNativeComponent
          key={-1}
          style={props.ListHeaderComponentStyle}
        >
          {invoker(props.ListHeaderComponent)}
        </ShadowListItemNativeComponent>
      ) : null}

      {data.length ? (
        data.map((item, index) => (
          <ShadowListItemNativeComponent key={index}>
            {props.renderItem({ item, index })}
          </ShadowListItemNativeComponent>
        ))
      ) : (
        <ShadowListItemNativeComponent key={0}>
          {invoker(props.ListEmptyComponent)}
        </ShadowListItemNativeComponent>
      )}

      {props.ListFooterComponent ? (
        <ShadowListItemNativeComponent
          key={data.length}
          style={props.ListFooterComponentStyle}
        >
          {invoker(props.ListFooterComponent)}
        </ShadowListItemNativeComponent>
      ) : null}
    </ShadowListContainerNativeComponent>
  );
};

export default React.forwardRef(ShadowListContainerWrapper);
