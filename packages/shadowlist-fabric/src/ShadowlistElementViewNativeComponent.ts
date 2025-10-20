import {
  codegenNativeComponent,
  type ViewProps,
  type CodegenTypes,
} from 'react-native';

interface NativeProps extends ViewProps {
  index: CodegenTypes.Int32;
}

export default codegenNativeComponent<NativeProps>('ShadowlistElementView');
