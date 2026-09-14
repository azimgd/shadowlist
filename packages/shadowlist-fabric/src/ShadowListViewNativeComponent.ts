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
  /*
   * Keys of the moved row and its drop-target neighbour. JS resolves each to
   * a current index against `data` before applying the move, so a data change between
   * gesture and drop can't reorder the wrong rows.
   */
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
  /*
   * Keys of decoration rows (date pills, unread dividers, reaction strips, padding) the
   * core must never auto-capture as the maintain-visible-content-position anchor. Keep these
   * keys STABLE and list them here instead of churning a row's key to "make MVCP ignore it":
   * the core anchors to the nearest stable content row, so decoration never perturbs scroll.
   */
  elementsAnchorIgnoreKeys: string[];
  /*
   * Ahead-of-time row measurements, as a JSON array of specs (see `ElementSizeSpec`).
   *
   * A row's height is normally learned only after JS renders it and Fabric lays it out, at
   * which point the core reflows everything after it -- continuously, for the whole scroll,
   * on a variable-height list. A spec lets native compute that height first, through the
   * same text engine the real layout will use, so geometry is right from the first frame:
   * no reflow churn, an honest scrollbar, and `scrollToIndex` that lands immediately.
   *
   * It is also what allows a row to be dematerialized before it has ever been mounted --
   * without a trusted size the materialization band has to keep every unmounted row alive.
   *
   * Partial by design. Describe the rows you can; anything absent (or malformed, or
   * containing inline images, which cannot be measured before layout) simply falls back to
   * the ordinary estimate. Send a window slightly ahead of `overscan`, not the whole
   * dataset: measuring 100k rows is the cost this prop exists to avoid.
   */
  elementsSizeSpecs?: CodegenTypes.WithDefault<string, ''>;
  inverted: boolean;
  horizontal: boolean;
  stickyHeader: boolean;
  stickyFooter: boolean;
  autoHideHeader: boolean;
  autoHideFooter: boolean;
  dragEnabled: boolean;
  stickyHeaderIndices: ReadonlyArray<CodegenTypes.Int32>;
  columns: CodegenTypes.Int32;
  /*
   * Overscan in viewport units: rows to measure/mount beyond the visible window on each
   * side, as a multiple of the window size. Consumed by the C++ core.
   */
  overscan: CodegenTypes.Double;
  /*
   * How far beyond the viewport rows stay mounted as native views, in viewport units.
   * `overscan` keeps rows reconciled (which is what avoids blank rows on a fast scroll);
   * this decides how many of those actually exist natively, which is what costs the UI
   * thread on every frame. Negative keeps the two identical.
   */
  nativeViewOverscan?: CodegenTypes.WithDefault<CodegenTypes.Double, -1.0>;
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
  /*
   * Whether JS actually listens for onScroll / onViewableIndicesChange.
   *
   * A DirectEventHandler prop tells the native side nothing about whether a handler was
   * supplied: the event emitter exists either way, so the core would compute and dispatch
   * these every scroll frame into the JS event queue even for a list that ignores them.
   * These flags let the core skip installing the observer entirely, which also skips
   * getViewableIndices' O(window) overlap scan.
   *
   * They matter for more than the wasted dispatch. Fabric coalesces a repeated event only
   * against the LAST event queued for the same target (EventQueue::enqueueEvent), so a
   * frame that emits visibleIndicesChange AND scroll leaves neither able to collapse and a
   * backlog builds one entry per frame. Emitting only what someone is listening to is what
   * keeps the coalescing effective.
   */
  scrollEventEnabled?: CodegenTypes.WithDefault<boolean, false>;
  viewableEventEnabled?: CodegenTypes.WithDefault<boolean, false>;
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
