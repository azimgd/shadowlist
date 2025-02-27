import React, { useCallback, useState, type Ref } from 'react';
import { StyleSheet, Text, type ViewStyle } from 'react-native';
import { SLContainer } from './SLContainer';
import { SLContent } from './SLContent';
import { SLElement } from './SLElement';
import type { ItemProp } from './SLContainer';
import type {
  OnViewableItemsChanged,
  SLContainerNativeCommands,
  SLContainerNativeProps,
} from './SLContainerNativeComponent';
import type { DirectEventHandler } from 'react-native/Libraries/Types/CodegenTypes';

type Component = React.ComponentType<any> | null | undefined;

const compareArrays = (
  prevViewableItems: number[],
  nextViewableItems: number[]
) =>
  prevViewableItems.length === nextViewableItems.length &&
  prevViewableItems
    .sort((a, b) => a - b)
    .every((v, i) => v === nextViewableItems[i]);

const invoker = (Component: Component) => {
  if (React.isValidElement(Component)) {
    return Component;
  } else if (Component) {
    return <Component />;
  }
  return null;
};

export type ShadowlistProps = {
  data: Array<ItemProp>;
  templates?: {
    [key: string]: () => React.ReactElement;
  };
  contentContainerStyle?: ViewStyle;
  ListHeaderComponent?: Component;
  ListHeaderComponentStyle?: ViewStyle;
  ListFooterComponent?: Component;
  ListFooterComponentStyle?: ViewStyle;
  ListEmptyComponent?: Component;
  ListEmptyComponentStyle?: ViewStyle;
};

export const Shadowlist = React.forwardRef(
  (
    props: Omit<SLContainerNativeProps, 'data'> & ShadowlistProps,
    forwardedRef: Ref<Partial<SLContainerNativeCommands>>
  ) => {
    /**
     * ListHeaderComponent
     */
    const ListHeaderComponent = (
      <SLElement
        style={props.ListHeaderComponentStyle}
        uniqueId="ListHeaderComponentUniqueId"
        key="ListHeaderComponentUniqueId"
      >
        {invoker(props.ListHeaderComponent)}
      </SLElement>
    );

    /**
     * ListFooterComponent
     */
    const ListFooterComponent = (
      <SLElement
        style={props.ListFooterComponentStyle}
        uniqueId="ListFooterComponentUniqueId"
        key="ListFooterComponentUniqueId"
      >
        {invoker(props.ListFooterComponent)}
      </SLElement>
    );

    /**
     * ListEmptyComponent
     */
    const ListEmptyComponent = (
      <SLElement
        style={props.ListEmptyComponentStyle}
        uniqueId="ListEmptyComponentUniqueId"
        key="ListEmptyComponentUniqueId"
      >
        {invoker(props.ListEmptyComponent)}
      </SLElement>
    );

    const templates = Object.entries(props.templates ?? {});
    const ListTemplatesComponent = templates.map(([templateKey, template]) => (
      <SLElement uniqueId={templateKey} key={templateKey}>
        {template()}
      </SLElement>
    ));

    /**
     * onViewableItemsChanged
     */
    const [viewableItems, setViewableItems] = useState<
      OnViewableItemsChanged['viewableItems']
    >([]);

    const handleViewableItemsChanged = useCallback<
      DirectEventHandler<OnViewableItemsChanged>
    >((event) => {
      event.persist();

      setViewableItems((state) => {
        const prevViewableItems = state.map(
          (viewableItem) => viewableItem.index
        );
        const nextViewableItems = event.nativeEvent.viewableItems.map(
          (viewableItem) => viewableItem.index
        );

        if (compareArrays(prevViewableItems, nextViewableItems)) {
          return state;
        }

        return event.nativeEvent.viewableItems;
      });
    }, []);

    /**
     * SLContentComponent
     */
    const SLContentComponent = <SLContent />;

    /**
     * ListDynamicComponent
     */
    const ListDynamicComponent = (
      <SLElement
        style={styles.ListDynamicComponent}
        uniqueId="ListDynamicComponentUniqueId"
        key="ListDynamicComponentUniqueId"
      >
        {viewableItems.map((viewableItem) => (
          <Text
            key={viewableItem.key}
            style={viewableItemStyle(viewableItem.origin, viewableItem.size)}
          >
            {viewableItem.key}
          </Text>
        ))}
      </SLElement>
    );

    return (
      <SLContainer
        {...props}
        ref={forwardedRef}
        style={props.contentContainerStyle}
        onViewableItemsChanged={handleViewableItemsChanged}
      >
        {SLContentComponent}
        {ListDynamicComponent}

        {ListHeaderComponent}
        {ListTemplatesComponent}
        {ListEmptyComponent}
        {ListFooterComponent}
      </SLContainer>
    );
  }
);

export const viewableItemStyle = (
  origin: OnViewableItemsChanged['viewableItems'][number]['origin'],
  size: OnViewableItemsChanged['viewableItems'][number]['size']
): ViewStyle => ({
  position: 'absolute',
  left: origin.x,
  top: origin.y,
  width: size.width,
  height: size.height,
});

const styles = StyleSheet.create({
  ListDynamicComponent: {
    ...StyleSheet.absoluteFillObject,
  },
});
