import React from 'react';
import { View, StyleSheet, type ViewStyle } from 'react-native';
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
  keyExtractor?: ((item: any, index: number) => string) | undefined;
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
    <ShadowListItemNativeComponent>
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
  const ListHeaderComponent = React.useMemo(() => {
    return props.ListHeaderComponent ? (
      <View style={props.ListHeaderComponentStyle}>
        {invoker(props.ListHeaderComponent)}
      </View>
    ) : null;
  }, [props.ListHeaderComponent, props.ListHeaderComponentStyle]);

  /**
   * ListFooterComponent
   */
  const ListFooterComponent = React.useMemo(() => {
    return props.ListFooterComponent ? (
      <View style={props.ListFooterComponentStyle}>
        {invoker(props.ListFooterComponent)}
      </View>
    ) : null;
  }, [props.ListFooterComponent, props.ListFooterComponentStyle]);

  /**
   * ListEmptyComponent
   */
  const ListEmptyComponent = React.useMemo(() => {
    return props.ListEmptyComponent ? (
      <View style={props.ListEmptyComponentStyle}>
        {invoker(props.ListEmptyComponent)}
      </View>
    ) : null;
  }, [props.ListEmptyComponent, props.ListEmptyComponentStyle]);

  /**
   * ListChildrenComponent
   */
  const ListChildrenComponent = React.useMemo(() => {
    return data.map((item, index) => (
      <ShadowListItemWrapper
        renderItem={props.renderItem}
        item={item}
        index={index}
        key={props.keyExtractor ? props.keyExtractor(item, index) : index}
      />
    ));
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [data, props.renderItem, props.keyExtractor]);

  return (
    <ShadowListContainerNativeComponent
      {...props}
      ref={instanceRef}
      style={[props.contentContainerStyle, containerStyle]}
    >
      <ShadowListContentNativeComponent
        style={contentStyle}
        inverted={props.inverted}
        horizontal={props.horizontal}
        hasListHeaderComponent={!!props.ListHeaderComponent}
        hasListFooterComponent={!!props.ListFooterComponent}
      >
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
