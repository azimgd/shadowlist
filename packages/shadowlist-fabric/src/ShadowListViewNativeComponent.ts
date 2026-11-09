import {
  codegenNativeComponent,
  codegenNativeCommands,
  type ViewProps,
  type CodegenTypes,
  type ColorValue,
} from 'react-native';

/*
 * The next rows to mount, by the keys at both ends. lowIndex and highIndex are where those rows
 * were in the keys native had, a hint that saves JS a lookup while the data is unchanged.
 * Sequence and generation go back to native in the mountedSequence and mountedGeneration props
 * once applied. A landed event only says a scroll command's rows can unmount.
 */
export type MountedRangeChangeEvent = {
  lowKey: string;
  highKey: string;
  lowIndex: CodegenTypes.Int32;
  highIndex: CodegenTypes.Int32;
  lowAtStart: boolean;
  highAtEnd: boolean;
  sequence: CodegenTypes.Double;
  generation: CodegenTypes.Double;
  landed: boolean;
};

/*
 * The pinned section header, the visible rows and, for each viewability config that changed,
 * the keys that became viewable or stopped. Codegen only takes an inline object type for an
 * array of objects.
 */
export type ViewabilityChangeEvent = {
  activeStickyIndex: CodegenTypes.Int32;
  visibleLow: CodegenTypes.Int32;
  visibleHigh: CodegenTypes.Int32;
  changes: {
    config: CodegenTypes.Int32;
    addedKeys: string[];
    removedKeys: string[];
  }[];
};

/*
 * The rows to load early and the rows no longer needed, by index in the keys native had and by
 * key, which finds them again in data that changed since.
 */
export type PrefetchChangeEvent = {
  prefetch: CodegenTypes.Int32[];
  cancel: CodegenTypes.Int32[];
  prefetchKeys: string[];
  cancelKeys: string[];
};

export type StartReachedEvent = {};
export type EndReachedEvent = {};
export type RefreshEvent = {};
export type RefreshSettleEvent = {};

export type NativeScrollEvent = {
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

export type ContentSizeChangeEvent = {
  width: CodegenTypes.Double;
  height: CodegenTypes.Double;
};

export type AnchorStateEvent = {
  found: boolean;
  key: string;
  offset: CodegenTypes.Double;
};

export type DragStartEvent = {
  key: string;
};

/*
 * A drop. Within the list, the row with sourceKey moved to where destinationKey was. A drop
 * in another list of the same drag board names that list in destinationListId and the place
 * the row goes in destinationIndex. Those two are '' and -1 for a drop within the list.
 * Codegen events in this file have no optional fields. Both are always sent.
 */
export type DragEndEvent = {
  sourceKey: string;
  destinationKey: string;
  destinationListId: string;
  destinationIndex: CodegenTypes.Int32;
};

export type AtEndChangeEvent = {
  atEnd: boolean;
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
  scrollToRow: (
    viewRef: React.ElementRef<ShadowListViewComponentType>,
    row: CodegenTypes.Int32,
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

/*
 * The row keys live in the list's native key store, written through the ShadowListStore
 * module: keysStore is the store's id and keysVersion the version this render wrote, see
 * ShadowListStoreJsi.h. sizeSpecsVersion names the row size specs written there the same way.
 * Without the module, keysStore is 0 and the keys come in rowKeys, the specs as JSON in
 * rowSizeSpecs. Lists that share a non-empty dragBoardId form one drag board, where a row
 * picked up in one list can drop in another. dragListId names this list on the board, and
 * dragListLabel is the name VoiceOver and TalkBack give it in a row's Move to action.
 */
interface NativeProps extends ViewProps {
  keysStore?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  keysVersion?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  sizeSpecsVersion?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  rowKeys?: string[];
  nonAnchorKeys: string[];
  rowSizeSpecs?: CodegenTypes.WithDefault<string, ''>;
  inverted: boolean;
  followAppends?: CodegenTypes.WithDefault<boolean, false>;
  horizontal: boolean;
  stickyHeader: boolean;
  stickyFooter: boolean;
  autoHideHeader: boolean;
  autoHideFooter: boolean;
  reorderEnabled: boolean;
  dragBoardId?: CodegenTypes.WithDefault<string, ''>;
  dragListId?: CodegenTypes.WithDefault<string, ''>;
  dragListLabel?: CodegenTypes.WithDefault<string, ''>;
  stickyIndices: ReadonlyArray<CodegenTypes.Int32>;
  numberOfColumns: CodegenTypes.Int32;
  overscan: CodegenTypes.Double;
  scrollIndex: CodegenTypes.Int32;
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
  progressViewOffset?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  viewableEventEnabled?: CodegenTypes.WithDefault<boolean, false>;
  viewableTimings?: ReadonlyArray<CodegenTypes.Double>;
  viewabilityInteracted?: CodegenTypes.WithDefault<boolean, false>;
  visibleRangeEventEnabled?: CodegenTypes.WithDefault<boolean, false>;
  mountedLow?: CodegenTypes.WithDefault<CodegenTypes.Int32, -1>;
  mountedHigh?: CodegenTypes.WithDefault<CodegenTypes.Int32, -1>;
  mountedSequence?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  mountedGeneration?: CodegenTypes.WithDefault<CodegenTypes.Double, 0>;
  seedPending?: CodegenTypes.WithDefault<boolean, false>;
  mountOverscanRows?: CodegenTypes.WithDefault<CodegenTypes.Int32, 4>;
  mountOverscanRowsLeading?: CodegenTypes.WithDefault<CodegenTypes.Int32, 10>;
  prefetchRows?: CodegenTypes.WithDefault<CodegenTypes.Int32, 0>;
  readonly onMountedRangeChange?: CodegenTypes.DirectEventHandler<MountedRangeChangeEvent>;
  readonly onViewabilityChange?: CodegenTypes.DirectEventHandler<ViewabilityChangeEvent>;
  readonly onPrefetchChange?: CodegenTypes.DirectEventHandler<PrefetchChangeEvent>;
  readonly onStartReached?: CodegenTypes.DirectEventHandler<StartReachedEvent>;
  readonly onEndReached?: CodegenTypes.DirectEventHandler<EndReachedEvent>;
  readonly onScroll?: CodegenTypes.DirectEventHandler<NativeScrollEvent>;
  readonly onScrollBeginDrag?: CodegenTypes.DirectEventHandler<NativeScrollEvent>;
  readonly onScrollEndDrag?: CodegenTypes.DirectEventHandler<NativeScrollEvent>;
  readonly onMomentumScrollBegin?: CodegenTypes.DirectEventHandler<NativeScrollEvent>;
  readonly onMomentumScrollEnd?: CodegenTypes.DirectEventHandler<NativeScrollEvent>;
  readonly onContentSizeChange?: CodegenTypes.DirectEventHandler<ContentSizeChangeEvent>;
  readonly onAnchorState?: CodegenTypes.DirectEventHandler<AnchorStateEvent>;
  readonly onRefresh?: CodegenTypes.DirectEventHandler<RefreshEvent>;
  readonly onRefreshSettle?: CodegenTypes.DirectEventHandler<RefreshSettleEvent>;
  readonly onDragStart?: CodegenTypes.DirectEventHandler<DragStartEvent>;
  readonly onDragEnd?: CodegenTypes.DirectEventHandler<DragEndEvent>;
  readonly onAtEndChange?: CodegenTypes.DirectEventHandler<AtEndChangeEvent>;
}

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: [
    'setStartReachedEnabled',
    'setEndReachedEnabled',
    'scrollToRow',
    'scrollToOffset',
    'scrollToEnd',
    'flashScrollIndicators',
    'requestAnchorState',
    'closeSwipeActions',
  ],
});

export default codegenNativeComponent<NativeProps>('ShadowListView');
