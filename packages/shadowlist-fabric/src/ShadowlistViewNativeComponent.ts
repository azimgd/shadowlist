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

/*
 * Pull-to-refresh. Fired when the user pulls past the refresh threshold at the start
 * of a vertical list (native UIRefreshControl on iOS, SwipeRefreshLayout on Android).
 */
export type OnRefresh = {};

export type OnScroll = {
  contentOffsetX: CodegenTypes.Double;
  contentOffsetY: CodegenTypes.Double;
};

/*
 * Drag-to-reorder lifecycle. The whole gesture - finger tracking, edge auto-scroll
 * and the make-room shuffle - runs on the native UI thread without mutating the JS
 * tree, so only these two boundaries are relayed back. `index` is the flat element
 * index the gesture picked up; `fromIndex`/`toIndex` are the picked-up element's
 * original and final indices for the single reorder applied on drop.
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
  /*
   * Auto-hiding header/footer: the bar pins to its edge, slides away as the user
   * scrolls toward the content and slides back as they scroll the other way
   * (direction-based). Handled natively on the UI thread like the sticky pin.
   */
  autoHideHeader: boolean;
  autoHideFooter: boolean;
  /*
   * Enables native long-press drag-to-reorder. The press-and-hold pickup, finger
   * tracking, edge auto-scroll and live shuffle are all handled natively; JS only
   * mirrors the resulting order and is notified through onDrag* events.
   */
  dragEnabled: boolean;
  /*
   * Element indices that are sticky section headers (ascending). Empty for a plain
   * list; a SectionList passes the flattened indices of its section-header rows.
   */
  stickyHeaderIndices: ReadonlyArray<CodegenTypes.Int32>;
  columns: CodegenTypes.Int32;
  containerOffsetIndex: CodegenTypes.Int32;
  /*
   * Bottom inset (px) added to the scroll view's scrollable region, used for
   * keyboard avoidance. The native side grows the bottom content inset by this
   * amount and slides the content up by the same delta so rows behind the keyboard
   * become visible. Driven from JS by the keyboard-avoiding hook (see Shadowlist's
   * keyboardAvoidingEnabled); 0 when no inset is applied. Vertical lists only.
   */
  contentInsetBottom: CodegenTypes.Double;
  /*
   * Pull-to-refresh. `refreshEnabled` installs the native refresh control (the JS
   * layer sets it from the presence of an onRefresh handler); `refreshing` is the
   * controlled spinner state; `refreshColor` tints the native indicator (the spinner
   * on iOS, the arc on Android). Vertical lists only.
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
