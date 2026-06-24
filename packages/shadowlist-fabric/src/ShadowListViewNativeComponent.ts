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
};

export type OnStartReached = {};
export type OnEndReached = {};
export type OnRefresh = {};
export type OnRefreshSettle = {};

export type OnScroll = {
  contentOffsetX: CodegenTypes.Double;
  contentOffsetY: CodegenTypes.Double;
};

export type OnDragStart = {
  key: string;
};

export type OnDragEnd = {
  // Keys of the moved row and its drop-target neighbour. JS resolves each to
  // a current index against `data` before applying the move, so a data change between
  // gesture and drop can't reorder the wrong rows.
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
  scrollToIndex: (
    viewRef: React.ElementRef<ShadowListViewComponentType>,
    index: CodegenTypes.Int32
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
}

interface NativeProps extends ViewProps {
  elementsAllKeys: string[];
  // Keys of decoration rows (date pills, unread dividers, reaction strips, padding) the
  // core must never auto-capture as the maintain-visible-content-position anchor. Keep these
  // keys STABLE and list them here instead of churning a row's key to "make MVCP ignore it":
  // the core anchors to the nearest stable content row, so decoration never perturbs scroll.
  elementsAnchorIgnoreKeys: string[];
  inverted: boolean;
  horizontal: boolean;
  stickyHeader: boolean;
  stickyFooter: boolean;
  autoHideHeader: boolean;
  autoHideFooter: boolean;
  dragEnabled: boolean;
  stickyHeaderIndices: ReadonlyArray<CodegenTypes.Int32>;
  columns: CodegenTypes.Int32;
  // Overscan in viewport units: rows to measure/mount beyond the visible window on each
  // side, as a multiple of the window size. Consumed by the C++ core.
  overscan: CodegenTypes.Double;
  containerOffsetIndex: CodegenTypes.Int32;
  contentInsetBottom: CodegenTypes.Double;
  refreshEnabled: boolean;
  refreshing: boolean;
  refreshColor?: ColorValue;
  startReachedThreshold: CodegenTypes.Double;
  endReachedThreshold: CodegenTypes.Double;
  viewablePercentThreshold: CodegenTypes.Double;
  snapToItem: boolean;
  snapToAlignment: CodegenTypes.Int32;
  readonly onVisibleIndicesChange?: CodegenTypes.DirectEventHandler<OnVisibleIndicesChange>;
  readonly onViewableIndicesChange?: CodegenTypes.DirectEventHandler<OnViewableIndicesChange>;
  readonly onStartReached?: CodegenTypes.DirectEventHandler<OnStartReached>;
  readonly onEndReached?: CodegenTypes.DirectEventHandler<OnEndReached>;
  readonly onScroll?: CodegenTypes.DirectEventHandler<OnScroll>;
  readonly onRefresh?: CodegenTypes.DirectEventHandler<OnRefresh>;
  readonly onRefreshSettle?: CodegenTypes.DirectEventHandler<OnRefreshSettle>;
  readonly onDragStart?: CodegenTypes.DirectEventHandler<OnDragStart>;
  readonly onDragEnd?: CodegenTypes.DirectEventHandler<OnDragEnd>;
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

export default codegenNativeComponent<NativeProps>('ShadowListView');
