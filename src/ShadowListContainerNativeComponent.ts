import React from 'react';
import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';
import codegenNativeCommands from 'react-native/Libraries/Utilities/codegenNativeCommands';
import type { ViewProps } from 'react-native';
import type {
  Int32,
  DirectEventHandler,
} from 'react-native/Libraries/Types/CodegenTypes';

export interface NativeProps extends ViewProps {
  inverted?: boolean;
  hasListHeaderComponent?: boolean;
  hasListFooterComponent?: boolean;
  initialScrollIndex?: Int32;
  onVisibleChange?: DirectEventHandler<
    Readonly<{
      start: Int32;
      end: Int32;
    }>
  >;
}

export interface NativeCommands {
  scrollToIndex: (
    viewRef: React.ElementRef<React.ComponentType>,
    index: Int32
  ) => void;
  scrollToOffset: (
    viewRef: React.ElementRef<React.ComponentType>,
    offset: Int32
  ) => void;
}

export const Commands = codegenNativeCommands<NativeCommands>({
  supportedCommands: ['scrollToIndex', 'scrollToOffset'],
});

export default codegenNativeComponent<NativeProps>('ShadowListContainer', {
  interfaceOnly: true,
});
