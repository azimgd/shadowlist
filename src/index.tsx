import React from 'react';
import { StyleSheet, type ViewStyle } from 'react-native';
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

export type ScrollToIndexOptions = {
  index: number;
  animated: boolean;
};

export type ScrollToOffsetOptions = {
  offset: number;
  animated: boolean;
};

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
    scrollToIndex: (options: ScrollToIndexOptions) => {
      Commands.scrollToIndex(
        instanceRef.current as never,
        options.index,
        options.animated
      );
    },
    scrollToOffset: (options: ScrollToOffsetOptions) => {
      Commands.scrollToOffset(
        instanceRef.current as never,
        options.offset,
        options.animated
      );
    },
  }));

  const data = React.useMemo(() => {
    return props.inverted ? props.data.reverse() : props.data;
  }, [props.inverted, props.data]);

  const baseStyle = props.horizontal ? styles.horizontal : styles.vertical;

  /**
   * ListHeaderComponent
   */
  const ListHeaderComponent = React.useMemo(
    () =>
      props.ListHeaderComponent ? (
        <ShadowListItemNativeComponent
          key="ListHeaderComponent"
          style={props.ListHeaderComponentStyle}
        >
          {invoker(props.ListHeaderComponent)}
        </ShadowListItemNativeComponent>
      ) : null,
    [props.ListHeaderComponent, props.ListHeaderComponentStyle]
  );

  /**
   * ListFooterComponent
   */
  const ListFooterComponent = React.useMemo(
    () =>
      props.ListFooterComponent ? (
        <ShadowListItemNativeComponent
          key="ListFooterComponent"
          style={props.ListFooterComponentStyle}
        >
          {invoker(props.ListFooterComponent)}
        </ShadowListItemNativeComponent>
      ) : null,
    [props.ListFooterComponent, props.ListFooterComponentStyle]
  );

  /**
   * ListEmptyComponent
   */
  const ListEmptyComponent = React.useMemo(
    () =>
      props.ListEmptyComponent ? (
        <ShadowListItemNativeComponent
          key="ListEmptyComponent"
          style={props.ListEmptyComponentStyle}
        >
          {invoker(props.ListEmptyComponent)}
        </ShadowListItemNativeComponent>
      ) : null,
    [props.ListEmptyComponent, props.ListEmptyComponentStyle]
  );

  /**
   * ListChildrenComponent
   */
  const ListChildrenComponent = React.useMemo(
    () =>
      data.map((item, index) => (
        <ShadowListItemNativeComponent key={index}>
          {props.renderItem({ item, index })}
        </ShadowListItemNativeComponent>
      )),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [data, props.renderItem]
  );

  return (
    <ShadowListContainerNativeComponent
      {...props}
      ref={instanceRef}
      hasListHeaderComponent={!!props.ListHeaderComponent}
      hasListFooterComponent={!!props.ListFooterComponent}
      style={[props.contentContainerStyle, baseStyle]}
    >
      {ListHeaderComponent}
      {data.length ? ListChildrenComponent : ListEmptyComponent}
      {ListFooterComponent}
    </ShadowListContainerNativeComponent>
  );
};

const styles = StyleSheet.create({
  vertical: {
    flexGrow: 1,
    flexShrink: 1,
    flexDirection: 'column',
    overflow: 'scroll',
  },
  horizontal: {
    flexGrow: 1,
    flexShrink: 1,
    flexDirection: 'row',
    overflow: 'scroll',
  },
});

export default React.forwardRef(ShadowListContainerWrapper);
