import React from 'react';
import { StyleSheet, type ViewStyle } from 'react-native';
import ShadowListContainerNativeComponent, {
  Commands,
  type NativeProps,
  type NativeCommands,
} from './ShadowListContainerNativeComponent';
import ShadowListContentNativeComponent from './ShadowListContentNativeComponent';
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

export type ShadowListContainerWrapperProps = {
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

export type ShadowListItemWrapperProps = {
  index: number;
  renderItem: ShadowListContainerWrapperProps['renderItem'];
  item: any;
};

const ShadowListItemWrapper = ({
  item,
  renderItem,
  index,
}: ShadowListItemWrapperProps) => {
  return (
    <ShadowListItemNativeComponent key={index}>
      {renderItem({ item, index })}
    </ShadowListItemNativeComponent>
  );
};

const ShadowListContainerWrapper = (
  props: NativeProps & ShadowListContainerWrapperProps,
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
    return props.data;
  }, [props.data]);

  const containerStyle = props.horizontal
    ? styles.containerHorizontal
    : styles.containerVertical;
  const contentStyle = props.horizontal
    ? props.inverted
      ? styles.contentHorizontalInverted
      : styles.contentHorizontal
    : props.inverted
      ? styles.contentVerticalInverted
      : styles.contentVertical;

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
        <ShadowListItemWrapper
          renderItem={props.renderItem}
          item={item}
          index={index}
          key={index}
        />
      )),
    [data, props.renderItem]
  );

  return (
    <ShadowListContainerNativeComponent
      {...props}
      ref={instanceRef}
      hasListHeaderComponent={!!props.ListHeaderComponent}
      hasListFooterComponent={!!props.ListFooterComponent}
      style={[props.contentContainerStyle, containerStyle]}
    >
      <ShadowListContentNativeComponent style={contentStyle}>
        {ListHeaderComponent}
        {data.length ? ListChildrenComponent : ListEmptyComponent}
        {ListFooterComponent}
      </ShadowListContentNativeComponent>
    </ShadowListContainerNativeComponent>
  );
};

const styles = StyleSheet.create({
  contentHorizontal: {
    flex: 1,
    flexDirection: 'row',
  },
  contentVertical: {
    flex: 1,
    flexDirection: 'column',
  },
  contentHorizontalInverted: {
    flex: 1,
    flexDirection: 'row-reverse',
    justifyContent: 'flex-end',
  },
  contentVerticalInverted: {
    flex: 1,
    flexDirection: 'column-reverse',
    justifyContent: 'flex-end',
  },
  containerVertical: {
    flex: 1,
    flexDirection: 'column',
    overflow: 'scroll',
  },
  containerHorizontal: {
    flex: 1,
    flexDirection: 'row',
    overflow: 'scroll',
  },
});

export default React.forwardRef(ShadowListContainerWrapper);
