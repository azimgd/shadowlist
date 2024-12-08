import React, { type Ref } from 'react';
import { type ViewStyle } from 'react-native';
import { SLContainer } from './SLContainer';
import { SLElement } from './SLElement';
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
  data: Array<any>;
  renderItem: () => React.ReactElement;
  keyExtractor: (item: any, index: number) => string;
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
    ref: Ref<Partial<SLContainerNativeCommands>>
  ) => {
    /**
     * ListHeaderComponent
     */
    const ListHeaderComponent = (
      <SLElement
        style={props.ListHeaderComponentStyle}
        uniqueId="ListHeaderComponentUniqueId"
        index={0}
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
        index={2}
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
        index={1}
      >
        {invoker(props.ListEmptyComponent)}
      </SLElement>
    );

    /**
     * ListChildrenComponent
     */
    const ListChildrenComponent = (
      <SLElement index={1} uniqueId="ListChildrenComponentUniqueId">
        {props.renderItem()}
      </SLElement>
    );

    return (
      <SLContainer
        {...props}
        ref={ref}
        style={[props.style, props.contentContainerStyle]}
        onVisibleChange={() => {}}
        onStartReached={() => {}}
        onEndReached={() => {}}
      >
        {[
          !props.inverted ? ListHeaderComponent : ListFooterComponent,
          props.data.length ? ListChildrenComponent : ListEmptyComponent,
          !props.inverted ? ListFooterComponent : ListHeaderComponent,
        ].toReversed()}
      </SLContainer>
    );
  }
);
