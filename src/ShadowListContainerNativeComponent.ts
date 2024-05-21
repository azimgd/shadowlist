import React from 'react';
import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';
import codegenNativeCommands from 'react-native/Libraries/Utilities/codegenNativeCommands';
import type { ViewProps } from 'react-native';
import type {
  Int32,
  DirectEventHandler,
  Double,
} from 'react-native/Libraries/Types/CodegenTypes';

export type OnBatchLayoutProps = { size: Int32 };
export type OnEndReachedProps = { distanceFromEnd: Int32 };

export interface NativeProps extends ViewProps {
  inverted?: boolean;
  horizontal?: boolean;
  hasListHeaderComponent?: boolean;
  hasListFooterComponent?: boolean;
  initialScrollIndex?: Int32;
  onVisibleChange?: DirectEventHandler<
    Readonly<{
      start: Int32;
      end: Int32;
    }>
  >;
  onBatchLayout?: DirectEventHandler<Readonly<OnBatchLayoutProps>>;
  onEndReached?: DirectEventHandler<Readonly<OnEndReachedProps>>;
  onEndReachedThreshold?: Double;
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
