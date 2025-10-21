import {
  codegenNativeComponent,
  codegenNativeCommands,
  type ViewProps,
  type CodegenTypes,
  type HostComponent,
} from 'react-native';

export type OnVisibleIndicesChange = {
  visibleStartIndex: CodegenTypes.Int32;
  visibleEndIndex: CodegenTypes.Int32;
};

interface NativeCommands {
  prependElements: (
    viewRef: React.ElementRef<HostComponent<NativeProps>>,
    size: CodegenTypes.Int32
  ) => void;
  appendElements: (
    viewRef: React.ElementRef<HostComponent<NativeProps>>,
    size: CodegenTypes.Int32
  ) => void;
}

interface NativeProps extends ViewProps {
  inverted: boolean;
  horizontal: boolean;
  readonly onVisibleIndicesChange: CodegenTypes.DirectEventHandler<OnVisibleIndicesChange>;
}

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: ['prependElements', 'appendElements'],
});

export default codegenNativeComponent<NativeProps>('ShadowlistView');
