import React from 'react';
import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';
import codegenNativeCommands from 'react-native/Libraries/Utilities/codegenNativeCommands';
import type { ViewProps } from 'react-native';
import type {
  Int32,
  DirectEventHandler,
  Double,
} from 'react-native/Libraries/Types/CodegenTypes';

export type OnEndReachedProps = { distanceFromEnd: Int32 };
export type OnStartReachedProps = { distanceFromStart: Int32 };
export type OnVisibleChildrenUpdate = {
  visibleStartIndex: Int32;
  visibleEndIndex: Int32;
};

export interface NativeProps extends ViewProps {
  inverted?: boolean;
  horizontal?: boolean;
  initialScrollIndex?: Int32;
  onVisibleChildrenUpdate?: DirectEventHandler<
    Readonly<OnVisibleChildrenUpdate>
  >;
  onEndReached?: DirectEventHandler<Readonly<OnEndReachedProps>>;
  onEndReachedThreshold?: Double;
  onStartReached?: DirectEventHandler<Readonly<OnStartReachedProps>>;
  onStartReachedThreshold?: Double;
}

export interface NativeCommands {
  scrollToIndex: (
    viewRef: React.ElementRef<React.ComponentType>,
    index: Int32,
    animated: boolean
  ) => void;
  scrollToOffset: (
    viewRef: React.ElementRef<React.ComponentType>,
    offset: Int32,
    animated: boolean
  ) => void;
}

export const Commands = codegenNativeCommands<NativeCommands>({
  supportedCommands: ['scrollToIndex', 'scrollToOffset'],
});

export default codegenNativeComponent<NativeProps>('ShadowListContainer', {
  interfaceOnly: true,
});
