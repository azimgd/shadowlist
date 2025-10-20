import {
  codegenNativeComponent,
  type ViewProps,
  type CodegenTypes,
} from 'react-native';

export type OnVisibleIndicesChange = {
  visibleStartIndex: CodegenTypes.Int32;
  visibleEndIndex: CodegenTypes.Int32;
};

interface NativeProps extends ViewProps {
  color?: string;
  readonly onVisibleIndicesChange: CodegenTypes.DirectEventHandler<OnVisibleIndicesChange>;
}

export default codegenNativeComponent<NativeProps>('ShadowlistView');
