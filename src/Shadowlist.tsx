import React, { type Ref } from 'react';
import { type ViewStyle } from 'react-native';
import { SLContainer } from './SLContainer';
import { SLContent } from './SLContent';
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
     * SLContentComponent
     */
    const SLContentComponent = <SLContent />;

    return (
      <SLContainer
        {...props}
        ref={forwardedRef}
        style={props.contentContainerStyle}
      >
        {SLContentComponent}
        {ListHeaderComponent}
        {ListTemplatesComponent}
        {ListEmptyComponent}
        {ListFooterComponent}
      </SLContainer>
    );
  }
);
