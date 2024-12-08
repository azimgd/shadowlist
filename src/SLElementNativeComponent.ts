import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';
import type { ViewProps } from 'react-native';
import type { Int32 } from 'react-native/Libraries/Types/CodegenTypes';

declare global {
  var __NATIVE_registerElementNode: (node: any) => void;
  var __NATIVE_unregisterElementNode: (node: any) => void;
}

export interface SLElementNativeProps extends ViewProps {
  index: Int32;
  uniqueId: string;
}

export default codegenNativeComponent<SLElementNativeProps>('SLElement', {
  interfaceOnly: true,
});
