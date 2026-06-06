import {
  codegenNativeComponent,
  codegenNativeCommands,
  type ViewProps,
  type CodegenTypes,
  type ColorValue,
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

/* Fired when the user pulls past the refresh threshold at the start of a vertical list. */
export type OnRefresh = {};

/* Fired once the pull-to-refresh spinner has fully retracted, so JS can apply a held prepend. */
export type OnRefreshSettle = {};

export type OnScroll = {
  contentOffsetX: CodegenTypes.Double;
  contentOffsetY: CodegenTypes.Double;
};

/*
 * Drag-to-reorder boundaries. `index` is the flat element index picked up;
 * `fromIndex`/`toIndex` are its original and final indices on drop.
 */
export type OnDragStart = {
  index: CodegenTypes.Int32;
};

export type OnDragEnd = {
  fromIndex: CodegenTypes.Int32;
  toIndex: CodegenTypes.Int32;
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
  /* Header/footer that pins to its edge and slides away/back based on scroll direction. */
  autoHideHeader: boolean;
  autoHideFooter: boolean;
  /* Enables long-press drag-to-reorder; JS mirrors the result via onDrag* events. */
  dragEnabled: boolean;
  /* Element indices that are sticky section headers (ascending); empty for a plain list. */
  stickyHeaderIndices: ReadonlyArray<CodegenTypes.Int32>;
  columns: CodegenTypes.Int32;
  containerOffsetIndex: CodegenTypes.Int32;
  /* Bottom inset (px) added for keyboard avoidance; 0 when none. Vertical lists only. */
  contentInsetBottom: CodegenTypes.Double;
  /*
   * Pull-to-refresh. `refreshEnabled` installs the refresh control; `refreshing` is
   * the controlled spinner state; `refreshColor` tints it. Vertical lists only.
   */
  refreshEnabled: boolean;
  refreshing: boolean;
  refreshColor?: ColorValue;
  startReachedThreshold: CodegenTypes.Double;
  endReachedThreshold: CodegenTypes.Double;
  viewablePercentThreshold: CodegenTypes.Double;
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

export default codegenNativeComponent<NativeProps>('ShadowlistView');
