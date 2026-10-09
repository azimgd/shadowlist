import {
  codegenNativeComponent,
  codegenNativeCommands,
  type ViewProps,
  type CodegenTypes,
  type ColorValue,
} from 'react-native';

export type OnVisibleIndicesChange = {
  visibleStartIndex: CodegenTypes.Int32;
  visibleEndIndex: CodegenTypes.Int32;
};

export type OnViewableIndicesChange = {
  viewableStartIndex: CodegenTypes.Int32;
  viewableEndIndex: CodegenTypes.Int32;
  ranges: CodegenTypes.Int32[];
};

export type OnStartReached = {};
export type OnEndReached = {};
export type OnRefresh = {};
export type OnRefreshSettle = {};

export type OnScroll = {
  contentOffsetX: CodegenTypes.Double;
  contentOffsetY: CodegenTypes.Double;
  contentOffset: Readonly<{
    x: CodegenTypes.Double;
    y: CodegenTypes.Double;
  }>;
  contentSize: Readonly<{
    width: CodegenTypes.Double;
    height: CodegenTypes.Double;
  }>;
  layoutMeasurement: Readonly<{
    width: CodegenTypes.Double;
    height: CodegenTypes.Double;
  }>;
  velocity: Readonly<{
    x: CodegenTypes.Double;
    y: CodegenTypes.Double;
  }>;
  contentInset: Readonly<{
    top: CodegenTypes.Double;
    left: CodegenTypes.Double;
    bottom: CodegenTypes.Double;
    right: CodegenTypes.Double;
  }>;
  zoomScale: CodegenTypes.Double;
};

export type OnContentSizeChange = {
  width: CodegenTypes.Double;
  height: CodegenTypes.Double;
};

export type OnAnchorState = {
  found: boolean;
  key: string;
  offset: CodegenTypes.Double;
};

export type OnDragStart = {
  key: string;
};

export type OnDragEnd = {
  fromKey: string;
  toKey: string;
};

type ShadowListViewComponentType = ReturnType<
  typeof codegenNativeComponent<NativeProps>
>;

interface NativeCommands {
  setStartReachedEnabled: (
    viewRef: React.ElementRef<ShadowListViewComponentType>,
    enabled: boolean
  ) => void;
  setEndReachedEnabled: (
    viewRef: React.ElementRef<ShadowListViewComponentType>,
    enabled: boolean
  ) => void;
  scrollToItem: (
    viewRef: React.ElementRef<ShadowListViewComponentType>,
    index: CodegenTypes.Int32,
    viewPosition: CodegenTypes.Double,
    viewOffset: CodegenTypes.Double,
    animated: boolean
  ) => void;
  scrollToOffset: (
    viewRef: React.ElementRef<ShadowListViewComponentType>,
    offset: CodegenTypes.Double,
    animated: boolean
  ) => void;
  scrollToEnd: (
    viewRef: React.ElementRef<ShadowListViewComponentType>,
    animated: boolean
  ) => void;
  flashScrollIndicators: (
    viewRef: React.ElementRef<ShadowListViewComponentType>
  ) => void;
  requestAnchorState: (
    viewRef: React.ElementRef<ShadowListViewComponentType>
  ) => void;
  closeSwipeActions: (
    viewRef: React.ElementRef<ShadowListViewComponentType>
  ) => void;
}

interface NativeProps extends ViewProps {
  elementsAllKeys: string[];
  elementsAnchorIgnoreKeys: string[];
  elementsSizeSpecs?: CodegenTypes.WithDefault<string, ''>;
  inverted: boolean;
  followAppends?: CodegenTypes.WithDefault<boolean, false>;
  horizontal: boolean;
  stickyHeader: boolean;
  stickyFooter: boolean;
  autoHideHeader: boolean;
  autoHideFooter: boolean;
  reorderEnabled: boolean;
  stickyIndices: ReadonlyArray<CodegenTypes.Int32>;
  numberOfColumns: CodegenTypes.Int32;
  overscan: CodegenTypes.Double;
  containerOffsetIndex: CodegenTypes.Int32;
  refreshEnabled: boolean;
  refreshing: boolean;
  refreshColor?: ColorValue;
  startReachedThreshold: CodegenTypes.Double;
  endReachedThreshold: CodegenTypes.Double;
  viewableRules: ReadonlyArray<CodegenTypes.Double>;
  snapToItem: boolean;
  snapAlignment: CodegenTypes.Int32;
  scrollEventEnabled?: CodegenTypes.WithDefault<boolean, false>;
  scrollEventThrottle?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  contentSizeEventEnabled?: CodegenTypes.WithDefault<boolean, false>;
  scrollEnabled?: CodegenTypes.WithDefault<boolean, true>;
  showsVerticalScrollIndicator?: CodegenTypes.WithDefault<boolean, true>;
  showsHorizontalScrollIndicator?: CodegenTypes.WithDefault<boolean, true>;
  bounces?: CodegenTypes.WithDefault<boolean, true>;
  decelerationRate?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  scrollsToTop?: CodegenTypes.WithDefault<boolean, true>;
  keyboardDismissMode?: CodegenTypes.WithDefault<
    'none' | 'on-drag' | 'interactive',
    'none'
  >;
  nestedScrollEnabled?: CodegenTypes.WithDefault<boolean, false>;
  refreshProgressViewOffset?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  viewableEventEnabled?: CodegenTypes.WithDefault<boolean, false>;
  readonly onVisibleIndicesChange?: CodegenTypes.DirectEventHandler<OnVisibleIndicesChange>;
  readonly onViewableIndicesChange?: CodegenTypes.DirectEventHandler<OnViewableIndicesChange>;
  readonly onStartReached?: CodegenTypes.DirectEventHandler<OnStartReached>;
  readonly onEndReached?: CodegenTypes.DirectEventHandler<OnEndReached>;
  readonly onScroll?: CodegenTypes.DirectEventHandler<OnScroll>;
  readonly onScrollBeginDrag?: CodegenTypes.DirectEventHandler<OnScroll>;
  readonly onScrollEndDrag?: CodegenTypes.DirectEventHandler<OnScroll>;
  readonly onMomentumScrollBegin?: CodegenTypes.DirectEventHandler<OnScroll>;
  readonly onMomentumScrollEnd?: CodegenTypes.DirectEventHandler<OnScroll>;
  readonly onContentSizeChange?: CodegenTypes.DirectEventHandler<OnContentSizeChange>;
  readonly onAnchorState?: CodegenTypes.DirectEventHandler<OnAnchorState>;
  readonly onRefresh?: CodegenTypes.DirectEventHandler<OnRefresh>;
  readonly onRefreshSettle?: CodegenTypes.DirectEventHandler<OnRefreshSettle>;
  readonly onDragStart?: CodegenTypes.DirectEventHandler<OnDragStart>;
  readonly onDragEnd?: CodegenTypes.DirectEventHandler<OnDragEnd>;
}

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: [
    'setStartReachedEnabled',
    'setEndReachedEnabled',
    'scrollToItem',
    'scrollToOffset',
    'scrollToEnd',
    'flashScrollIndicators',
    'requestAnchorState',
    'closeSwipeActions',
  ],
});

export default codegenNativeComponent<NativeProps>('ShadowListView');
