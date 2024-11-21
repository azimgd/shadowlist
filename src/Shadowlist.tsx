import React, { type Ref } from 'react';
import { type ViewStyle } from 'react-native';
import { SLContainer } from './SLContainer';
import type {
  SLContainerNativeCommands,
  SLContainerNativeProps,
} from './SLContainerNativeComponent';
import SLElementNativeComponent from './SLElementNativeComponent';

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
  data: any[];
  renderItem: (payload: { item: any; index: number }) => React.ReactElement;
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
    props: SLContainerNativeProps & ShadowlistProps,
    ref: Ref<Partial<SLContainerNativeCommands>>
  ) => {
    /**
     * ListHeaderComponent
     */
    const ListHeaderComponent = React.useMemo(
      () => (
        <SLElementNativeComponent
          style={props.ListHeaderComponentStyle}
          uniqueId="ListHeaderComponentUniqueId"
          index={-1}
        >
          {invoker(props.ListHeaderComponent)}
        </SLElementNativeComponent>
      ),
      [props.ListHeaderComponent, props.ListHeaderComponentStyle]
    );

    /**
     * ListFooterComponent
     */
    const ListFooterComponent = React.useMemo(
      () => (
        <SLElementNativeComponent
          style={props.ListFooterComponentStyle}
          uniqueId="ListFooterComponentUniqueId"
          index={-2}
        >
          {invoker(props.ListFooterComponent)}
        </SLElementNativeComponent>
      ),
      [props.ListFooterComponent, props.ListFooterComponentStyle]
    );

    /**
     * ListEmptyComponent
     */
    const ListEmptyComponent = React.useMemo(
      () => (
        <SLElementNativeComponent
          style={props.ListEmptyComponentStyle}
          uniqueId="ListEmptyComponentUniqueId"
          index={-3}
        >
          {invoker(props.ListEmptyComponent)}
        </SLElementNativeComponent>
      ),
      [props.ListEmptyComponent, props.ListEmptyComponentStyle]
    );

    /**
     * ListChildrenComponent
     */
    const ListChildrenComponent = React.useMemo(() => {
      return props.data.map((item, index) => {
        const uniqueId = props.keyExtractor(item, index);
        return (
          <SLElementNativeComponent
            index={index}
            uniqueId={uniqueId}
            key={uniqueId}
          >
            {props.renderItem({ item, index })}
          </SLElementNativeComponent>
        );
      });
      // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [props.data, props.renderItem, props.keyExtractor]);

    return (
      <SLContainer style={props.style} ref={ref} {...props}>
        {!props.inverted ? ListHeaderComponent : ListFooterComponent}
        {props.data.length ? ListChildrenComponent : ListEmptyComponent}
        {!props.inverted ? ListFooterComponent : ListHeaderComponent}
      </SLContainer>
    );
  }
);
