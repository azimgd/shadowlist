import React from 'react';
import { StyleSheet } from 'react-native';
import SLContainerNativeComponent, {
  type SLContainerNativeProps,
  type SLContainerNativeCommands,
} from './SLContainerNativeComponent';

export type SLContainerWrapperProps = {};

export type SLContainerInstance = InstanceType<
  typeof SLContainerNativeComponent
>;

const SLContainerWrapper = (
  props: SLContainerNativeProps & SLContainerWrapperProps,
  forwardedRef: React.Ref<Partial<SLContainerNativeCommands>>
) => {
  const instanceRef = React.useRef<SLContainerInstance>(null);

  React.useImperativeHandle(forwardedRef, () => ({
    scrollToIndex: () => {},
    scrollToOffset: () => {},
  }));

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
