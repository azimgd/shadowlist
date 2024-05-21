import React from 'react';
import {
  StyleSheet,
  type ViewStyle,
  type NativeSyntheticEvent,
} from 'react-native';
import EventEmitter from 'eventemitter3';
import ShadowListContainerNativeComponent, {
  Commands,
  type NativeProps,
  type NativeCommands,
  type OnBatchLayoutProps,
} from './ShadowListContainerNativeComponent';
import ShadowListItemNativeComponent from './ShadowListItemNativeComponent';

const emitter = new EventEmitter();
const BATCHER_SIZE = 50;

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
  /**
   * Always layout this component if it is within the first BATCHER_SIZE items.
   */
  const [isReady, setIsReady] = React.useState(index < BATCHER_SIZE);

  /**
   * Listens for the 'onBatchLayout' event.
   * Sets the component to ready when its index is within the batch layout meaning that it's layed out.
   */
  React.useEffect(() => {
    const listener = (batchLayoutSize: number) => {
      const shouldRenderListItem = index <= batchLayoutSize + BATCHER_SIZE;
      if (!shouldRenderListItem) return;
      emitter.off('onBatchLayout', listener);
      setIsReady(true);
    };

    emitter.on('onBatchLayout', listener);

    return () => {
      emitter.off('onBatchLayout', listener);
    };
  }, [index]);

  if (!isReady) {
    return null;
  }

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
        <ShadowListItemWrapper
          renderItem={props.renderItem}
          item={item}
          index={index}
          key={index}
        />
      )),
    [data, props.renderItem]
  );

  /**
   * Notifies all subscribed components about a new layout batch size.
   * It uses an event notifier to reduce re-renders of the parent list component.
   */
  const handleBatchLayout = React.useCallback(
    (event: NativeSyntheticEvent<OnBatchLayoutProps>) => {
      emitter.emit('onBatchLayout', event.nativeEvent.size);
    },
    []
  );

  return (
    <ShadowListContainerNativeComponent
      {...props}
      ref={instanceRef}
      hasListHeaderComponent={!!props.ListHeaderComponent}
      hasListFooterComponent={!!props.ListFooterComponent}
      style={[props.contentContainerStyle, baseStyle]}
      onBatchLayout={handleBatchLayout}
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
