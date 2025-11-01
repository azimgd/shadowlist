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

export type OnStartReached = {};

export type OnEndReached = {};

interface NativeCommands {
  prependElements: (
    viewRef: React.ElementRef<HostComponent<NativeProps>>,
    size: CodegenTypes.Int32
  ) => void;
  appendElements: (
    viewRef: React.ElementRef<HostComponent<NativeProps>>,
    size: CodegenTypes.Int32
  ) => void;
  setStartReachedEnabled: (
    viewRef: React.ElementRef<HostComponent<NativeProps>>,
    enabled: boolean
  ) => void;
  setEndReachedEnabled: (
    viewRef: React.ElementRef<HostComponent<NativeProps>>,
    enabled: boolean
  ) => void;
}

interface NativeProps extends ViewProps {
  elementsAllKeys: string[];
  elementsHeadKey: string | undefined;
  elementsTailKey: string | undefined;
  inverted: boolean;
  horizontal: boolean;
  readonly onVisibleIndicesChange?: CodegenTypes.DirectEventHandler<OnVisibleIndicesChange>;
  readonly onStartReached?: CodegenTypes.DirectEventHandler<OnStartReached>;
  readonly onEndReached?: CodegenTypes.DirectEventHandler<OnEndReached>;
}

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: [
    'prependElements',
    'appendElements',
    'setStartReachedEnabled',
    'setEndReachedEnabled',
  ],
});

export default codegenNativeComponent<NativeProps>('ShadowlistView');
