import React from 'react';
import { StyleSheet } from 'react-native';
import SLContainerNativeComponent, {
  Commands,
  type ScrollToIndexOptions,
  type ScrollToOffsetOptions,
  type SLContainerNativeProps,
  type SLContainerNativeCommands,
} from './SLContainerNativeComponent';

export type ItemProp = {
  id: string;
  [key: string]: any;
};

export type SLContainerWrapperProps = {
  data: Array<ItemProp>;
};

export type SLContainerInstance = InstanceType<
  typeof SLContainerNativeComponent
>;

export type SLContainerRef = {
  scrollToIndex: (options: ScrollToIndexOptions) => void;
  scrollToOffset: (offset: ScrollToOffsetOptions) => void;
};

const SLContainerWrapper = (
  props: Omit<SLContainerNativeProps, 'data'> & SLContainerWrapperProps,
  forwardedRef: React.Ref<Partial<SLContainerNativeCommands>>
) => {
  const instanceRef = React.useRef<SLContainerInstance | null>(null);

  React.useImperativeHandle<Partial<SLContainerNativeCommands>, SLContainerRef>(
    forwardedRef,
    () => ({
      scrollToIndex: (options: ScrollToIndexOptions) => {
        Commands.scrollToIndex(
          instanceRef.current as never,
          options.index,
          options.animated || false
        );
      },
      scrollToOffset: (options: ScrollToOffsetOptions) => {
        Commands.scrollToOffset(
          instanceRef.current as never,
          options.offset,
          options.animated || false
        );
      },
    })
  );

  const containerStyle = props.horizontal
    ? styles.containerHorizontal
    : styles.containerVertical;

  return (
    <SLContainerNativeComponent
      {...props}
      data={JSON.stringify(props.data)}
      style={[containerStyle, props.style]}
      ref={instanceRef}
    >
      {props.children}
    </SLContainerNativeComponent>
  );
};

const styles = StyleSheet.create({
  containerVertical: {
    height: '100%',
    flexDirection: 'column',
  },
  containerHorizontal: {
    width: '100%',
    flexDirection: 'row',
  },
});

export const SLContainer = React.forwardRef(SLContainerWrapper);
