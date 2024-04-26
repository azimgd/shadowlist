import React from 'react';
import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';
import codegenNativeCommands from 'react-native/Libraries/Utilities/codegenNativeCommands';
import type { ViewProps } from 'react-native';
import type { Int32 } from 'react-native/Libraries/Types/CodegenTypes';

export interface NativeProps extends ViewProps {}

export interface NativeCommands {
  scrollToIndex: (
    viewRef: React.ElementRef<React.ComponentType>,
    index: Int32
  ) => void;
}

export const Commands = codegenNativeCommands<NativeCommands>({
  supportedCommands: ['scrollToIndex'],
});

export default codegenNativeComponent<NativeProps>('CraigsListContainer');
