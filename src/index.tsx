import React from 'react';
import { StyleSheet } from 'react-native';
import SLContainerNativeComponent, {
  Commands,
  type ScrollToIndexOptions,
  type ScrollToOffsetOptions,
  type SLContainerNativeProps,
  type SLContainerNativeCommands,
} from './SLContainerNativeComponent';

export type SLContainerWrapperProps = {};

export type SLContainerInstance = InstanceType<
  typeof SLContainerNativeComponent
>;

export type SLContainerRef = {
  scrollToIndex: (options: ScrollToIndexOptions) => void;
  scrollToOffset: (offset: ScrollToOffsetOptions) => void;
};

const SLContainerWrapper = (
  props: SLContainerNativeProps & SLContainerWrapperProps,
  forwardedRef: React.Ref<Partial<SLContainerNativeCommands>>
) => {
  const instanceRef = React.useRef<SLContainerInstance>(null);

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
      ref={instanceRef}
      style={containerStyle}
    >
      {props.children}
    </SLContainerNativeComponent>
  );
};

const styles = StyleSheet.create({
  containerVertical: {
    flex: 1,
    flexDirection: 'column',
  },
  containerHorizontal: {
    flex: 1,
    flexDirection: 'row',
  },
});

export const SLContainer = React.forwardRef(SLContainerWrapper);
