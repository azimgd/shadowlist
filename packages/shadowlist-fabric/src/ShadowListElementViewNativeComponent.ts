import {
  codegenNativeComponent,
  type ViewProps,
  type CodegenTypes,
} from 'react-native';

interface NativeProps extends ViewProps {
  index: CodegenTypes.Int32;
  elementKey?: string;
}

export default codegenNativeComponent<NativeProps>('ShadowListElementView');
