import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';
import type { ViewProps } from 'react-native';
import type { Int32 } from 'react-native/Libraries/Types/CodegenTypes';

export interface SLContainerNativeProps extends ViewProps {
  inverted?: boolean;
  horizontal?: boolean;
  initialNumToRender?: Int32;
  initialScrollIndex?: Int32;
}

export interface SLContainerNativeCommands {
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

export type NativeCommands = SLContainerNativeCommands;

export default codegenNativeComponent<SLContainerNativeProps>('SLContainer');
