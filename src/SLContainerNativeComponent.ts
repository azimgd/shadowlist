import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';
import type { ViewProps } from 'react-native';
import type { Int32 } from 'react-native/Libraries/Types/CodegenTypes';

interface NativeProps extends ViewProps {
  inverted?: boolean;
  horizontal?: boolean;
  initialNumToRender?: Int32;
  initialScrollIndex?: Int32;
}

export default codegenNativeComponent<NativeProps>('SLContainer');
