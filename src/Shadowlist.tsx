import React, { type Ref } from 'react';
import { View, type ViewStyle } from 'react-native';
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
  keyExtractor?: ((item: any, index: number) => string) | undefined;
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
      return props.data.map((item, index) => (
        <SLElementNativeComponent index={index} key={index}>
          {props.renderItem({ item, index })}
        </SLElementNativeComponent>
      ));
      // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [props.data, props.renderItem, props.keyExtractor]);

    ListHeaderComponent;
    ListFooterComponent;

    return (
      <SLContainer style={props.style} ref={ref} {...props}>
        {props.data.length ? ListChildrenComponent : ListEmptyComponent}
      </SLContainer>
    );
  }
);
