import React, { type Ref } from 'react';
import { type ViewStyle } from 'react-native';
import { SLContainer } from './SLContainer';
import { SLElement } from './SLElement';
import type { ItemProp } from './SLContainer';
import type {
  SLContainerNativeCommands,
  SLContainerNativeProps,
} from './SLContainerNativeComponent';

type Component = React.ComponentType<any> | null | undefined;

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
  renderItem: () => React.ReactElement;
  keyExtractor: (item: ItemProp, index: number) => string;
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

    /**
     * ListChildrenComponent
     */
    const ListChildrenComponent = (
      <SLElement
        uniqueId="ListChildrenComponentUniqueId"
        key="ListChildrenComponentUniqueId"
      >
        {props.renderItem()}
      </SLElement>
    );

    return (
      <SLContainer
        {...props}
        ref={forwardedRef}
        style={[props.style, props.contentContainerStyle]}
      >
        {ListHeaderComponent}
        {props.data.length ? ListChildrenComponent : ListEmptyComponent}
        {ListFooterComponent}
      </SLContainer>
    );
  }
);
