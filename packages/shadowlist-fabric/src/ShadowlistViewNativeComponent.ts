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

export type OnViewableIndicesChange = {
  viewableStartIndex: CodegenTypes.Int32;
  viewableEndIndex: CodegenTypes.Int32;
};

export type OnStartReached = {};

export type OnEndReached = {};

export type OnScroll = {
  contentOffsetX: CodegenTypes.Double;
  contentOffsetY: CodegenTypes.Double;
};

interface NativeCommands {
  setStartReachedEnabled: (
    viewRef: React.ElementRef<HostComponent<NativeProps>>,
    enabled: boolean
  ) => void;
  setEndReachedEnabled: (
    viewRef: React.ElementRef<HostComponent<NativeProps>>,
    enabled: boolean
  ) => void;
  scrollToIndex: (
    viewRef: React.ElementRef<HostComponent<NativeProps>>,
    index: CodegenTypes.Int32
  ) => void;
  scrollToOffset: (
    viewRef: React.ElementRef<HostComponent<NativeProps>>,
    offset: CodegenTypes.Double,
    animated: boolean
  ) => void;
  scrollToEnd: (
    viewRef: React.ElementRef<HostComponent<NativeProps>>,
    animated: boolean
  ) => void;
}

interface NativeProps extends ViewProps {
  elementsAllKeys: string[];
  inverted: boolean;
  horizontal: boolean;
  stickyHeader: boolean;
  stickyFooter: boolean;
  /*
   * Element indices that are sticky section headers (ascending). Empty for a plain
   * list; a SectionList passes the flattened indices of its section-header rows.
   */
  stickyHeaderIndices: ReadonlyArray<CodegenTypes.Int32>;
  columns: CodegenTypes.Int32;
  containerOffsetIndex: CodegenTypes.Int32;
  startReachedThreshold: CodegenTypes.Double;
  endReachedThreshold: CodegenTypes.Double;
  viewablePercentThreshold: CodegenTypes.Double;
  readonly onVisibleIndicesChange?: CodegenTypes.DirectEventHandler<OnVisibleIndicesChange>;
  readonly onViewableIndicesChange?: CodegenTypes.DirectEventHandler<OnViewableIndicesChange>;
  readonly onStartReached?: CodegenTypes.DirectEventHandler<OnStartReached>;
  readonly onEndReached?: CodegenTypes.DirectEventHandler<OnEndReached>;
  readonly onScroll?: CodegenTypes.DirectEventHandler<OnScroll>;
}

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: [
    'setStartReachedEnabled',
    'setEndReachedEnabled',
    'scrollToIndex',
    'scrollToOffset',
    'scrollToEnd',
  ],
});

export default codegenNativeComponent<NativeProps>('ShadowlistView');
