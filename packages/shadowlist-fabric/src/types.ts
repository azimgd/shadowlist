import type { ComponentType, ReactElement } from 'react';
import type {
  ViewStyle,
  TextStyle,
  ColorValue,
  AccessibilityRole,
  StyleProp,
  Insets,
} from 'react-native';
import type { OnScroll } from './ShadowListViewNativeComponent';

export interface ViewToken<ItemT> {
  item: ItemT;
  index: number;
  key: string;
  isViewable: boolean;
}

export interface ScrollToIndexParams {
  index: number;
  animated?: boolean;
  viewOffset?: number;
  viewPosition?: number;
}

export interface ScrollToItemParams<ItemT = unknown> {
  item: ItemT;
  animated?: boolean;
  viewOffset?: number;
  viewPosition?: number;
}

export interface ScrollToIndexFailedInfo {
  index: number;
  highestMeasuredFrameIndex: number;
  averageItemLength: number;
}

export interface AnchorState {
  key: string;
  offset: number;
}

export interface ShadowListCommands {
  setStartReachedEnabled: (enabled: boolean) => void;
  setEndReachedEnabled: (enabled: boolean) => void;
  scrollToItem: (params: ScrollToItemParams) => void;
  scrollToIndex: (params: ScrollToIndexParams) => void;
  scrollToOffset: {
    (offset: number, animated?: boolean): void;
    (params: { offset: number; animated?: boolean }): void;
  };
  scrollToEnd: {
    (animated?: boolean): void;
    (params: { animated?: boolean }): void;
  };
  flashScrollIndicators: () => void;
  recordInteraction: () => void;
  getNativeScrollRef: () => unknown;
  getScrollResponder: () => unknown;
  getScrollableNode: () => number | null;
  getItemSize: (key: string) => number | undefined;
  getItemSizes: () => ReadonlyMap<string, number>;
  selectItem: (index: number) => void;
  deselectItem: (index: number) => void;
  getSelectedIndices: () => number[];
  closeSwipeActions: () => void;
  getAnchorState: () => Promise<AnchorState | null>;
  restoreAnchorState: (state: AnchorState) => void;
}

export interface ViewabilityConfig {
  itemVisiblePercentThreshold?: number;
  viewAreaCoveragePercentThreshold?: number;
  minimumViewTime?: number;
  waitForInteraction?: boolean;
}

export interface ViewableItemsChangedInfo<ItemT> {
  viewableItems: ViewToken<ItemT>[];
  changed: ViewToken<ItemT>[];
}

export interface ViewabilityConfigCallbackPair<ItemT> {
  viewabilityConfig: ViewabilityConfig;
  onViewableItemsChanged:
    | ((info: ViewableItemsChangedInfo<ItemT>) => void)
    | null
    | undefined;
}

/*
 * What a row can do to the separators next to it. leading is the
 * separator above the row, trailing the one below.
 */
export interface Separators {
  highlight: () => void;
  unhighlight: () => void;
  updateProps: (
    select: 'leading' | 'trailing',
    newProps: Record<string, unknown>
  ) => void;
}

export interface ItemSeparatorProps<ItemT> {
  highlighted: boolean;
  leadingItem: ItemT;
  trailingItem: ItemT | undefined;
}

export interface RenderItemInfo<ItemT> {
  item: ItemT;
  index: number;
  separators: Separators;
  selected: boolean;
  select: () => void;
  deselect: () => void;
}

export interface SwipeAction<ItemT> {
  title: string;
  style?: 'normal' | 'destructive';
  backgroundColor?: ColorValue;
  onPress: (info: { item: ItemT; index: number }) => void | Promise<unknown>;
}

export interface SwipeActionsConfiguration<ItemT> {
  actions: ReadonlyArray<SwipeAction<ItemT>>;
  performsFirstActionWithFullSwipe?: boolean;
}

export interface ContextMenuAction<ItemT> {
  title: string;
  style?: 'normal' | 'destructive';
  disabled?: boolean;
  systemImage?: string;
  onPress: (info: { item: ItemT; index: number }) => void;
}

export interface ContextMenu<ItemT> {
  title?: string;
  actions: ReadonlyArray<ContextMenuAction<ItemT>>;
}

export interface PrefetchDataSource {
  prefetchItems: (indices: number[]) => void;
  cancelPrefetchingForItems?: (indices: number[]) => void;
}

export type ScrollEvent = { nativeEvent: OnScroll };

export interface ItemSizeSpec {
  text: string;

  fontSize?: number;
  fontFamily?: string;
  fontWeight?: TextStyle['fontWeight'];
  fontStyle?: 'normal' | 'italic';
  lineHeight?: number;
  letterSpacing?: number;
  numberOfLines?: number;

  insetWidth?: number;
  insetHeight?: number;

  widthFraction?: number;

  fixedHeight?: number;
}

export interface ShadowListProps<ItemT> {
  data: ReadonlyArray<ItemT>;
  renderItem: (info: RenderItemInfo<ItemT>) => ReactElement;
  keyExtractor?: (item: ItemT, index: number) => string;
  style?: StyleProp<ViewStyle>;
  itemStyle?: StyleProp<ViewStyle>;
  accessible?: boolean;
  accessibilityLabel?: string;
  accessibilityRole?: AccessibilityRole;
  accessibilityHint?: string;
  testID?: string;
  inverted?: boolean;
  followAppends?: boolean;
  horizontal?: boolean;
  stickyHeader?: boolean;
  stickyFooter?: boolean;
  autoHideHeader?: boolean;
  autoHideFooter?: boolean;
  reorderEnabled?: boolean;
  onMoveItem?: (info: {
    sourceIndex: number;
    destinationIndex: number;
    data: ItemT[];
  }) => void;
  stickyIndices?: ReadonlyArray<number>;
  renderStickyHeaderOverlay?: (activeIndex: number) => ReactElement | null;
  numberOfColumns?: number;
  overscan?: number;
  mountOverscanRows?: number;
  mountOverscanRowsLeading?: number;
  getItemSizeSpec?: (
    item: ItemT,
    index: number
  ) => ItemSizeSpec | null | undefined;
  measureLookaheadRows?: number;
  persistentKeys?: ReadonlyArray<string>;
  nonAnchorKeys?: ReadonlyArray<string>;
  initialScrollIndex?: number | null;
  scrollIndex?: number;
  trackItemSizes?: boolean;
  extraData?: unknown;
  refreshing?: boolean;
  onRefresh?: () => void;
  refreshColor?: ColorValue;
  progressViewOffset?: number;
  initialNumToRender?: number;
  onStartReached?: () => void;
  onEndReached?: () => void;
  onStartReachedThreshold?: number;
  onEndReachedThreshold?: number;
  onScroll?: (event: ScrollEvent) => void;
  onScrollBeginDrag?: (event: ScrollEvent) => void;
  onScrollEndDrag?: (event: ScrollEvent) => void;
  onMomentumScrollBegin?: (event: ScrollEvent) => void;
  onMomentumScrollEnd?: (event: ScrollEvent) => void;
  onContentSizeChange?: (width: number, height: number) => void;
  scrollEventThrottle?: number;
  onScrollToIndexFailed?: (info: ScrollToIndexFailedInfo) => void;
  scrollEnabled?: boolean;
  showsVerticalScrollIndicator?: boolean;
  showsHorizontalScrollIndicator?: boolean;
  bounces?: boolean;
  decelerationRate?: 'normal' | 'fast' | number;
  scrollsToTop?: boolean;
  keyboardDismissMode?: 'none' | 'on-drag' | 'interactive';
  keyboardShouldPersistTaps?: boolean | 'always' | 'never' | 'handled';
  nestedScrollEnabled?: boolean;
  contentContainerStyle?: StyleProp<ViewStyle>;
  contentInset?: Insets;
  snapToItem?: boolean;
  snapAlignment?: 'start' | 'center' | 'end';
  viewabilityConfig?: ViewabilityConfig;
  onViewableItemsChanged?: (info: ViewableItemsChangedInfo<ItemT>) => void;
  viewabilityConfigCallbackPairs?: ReadonlyArray<
    ViewabilityConfigCallbackPair<ItemT>
  >;
  allowsMultipleSelection?: boolean;
  selectedKeys?: ReadonlyArray<string>;
  onSelectionChange?: (selectedKeys: string[]) => void;
  leadingSwipeActionsForItem?: (info: {
    item: ItemT;
    index: number;
  }) => SwipeActionsConfiguration<ItemT> | null | undefined;
  trailingSwipeActionsForItem?: (info: {
    item: ItemT;
    index: number;
  }) => SwipeActionsConfiguration<ItemT> | null | undefined;
  contextMenuForItem?: (info: {
    item: ItemT;
    index: number;
  }) => ContextMenu<ItemT> | null | undefined;
  prefetchDataSource?: PrefetchDataSource;
  prefetchRows?: number;
  ItemSeparatorComponent?:
    | ReactElement
    | ComponentType<ItemSeparatorProps<ItemT>>
    | null;
  ListHeaderComponent?: ReactElement | (() => ReactElement | null) | null;
  ListHeaderComponentStyle?: StyleProp<ViewStyle>;
  ListFooterComponent?: ReactElement | (() => ReactElement | null) | null;
  ListFooterComponentStyle?: StyleProp<ViewStyle>;
  ListEmptyComponent?: ReactElement | (() => ReactElement | null) | null;
  columnWrapperStyle?: StyleProp<ViewStyle>;
}

/*
 * SectionList types. A section holds its data plus any fields of your own, and can
 * have a stable key.
 */
export interface SectionBase<ItemT, SectionT = object> {
  data: ReadonlyArray<ItemT>;
  key?: string;
  renderItem?: SectionListRenderItem<ItemT, SectionT>;
  keyExtractor?: (item: ItemT, index: number) => string;
}

export type SectionListData<ItemT, SectionT = object> = SectionT &
  SectionBase<ItemT, SectionT>;

export interface SectionListRenderItemInfo<ItemT, SectionT = object> {
  item: ItemT;
  index: number;
  section: SectionListData<ItemT, SectionT>;
  separators: Separators;
}

export type SectionListRenderItem<ItemT, SectionT = object> = (
  info: SectionListRenderItemInfo<ItemT, SectionT>
) => ReactElement | null;

/*
 * ShadowList props that SectionList and TreeList pass through as is. Props that point at
 * rows are left out, since those lists render flattened rows the caller never sees. They
 * define their own version of such a prop or don't offer it.
 */
export type ShadowListForwardedProps = Omit<
  ShadowListProps<unknown>,
  | 'data'
  | 'renderItem'
  | 'keyExtractor'
  | 'getItemSizeSpec'
  | 'stickyIndices'
  | 'renderStickyHeaderOverlay'
  | 'reorderEnabled'
  | 'onMoveItem'
  | 'viewabilityConfig'
  | 'onViewableItemsChanged'
  | 'viewabilityConfigCallbackPairs'
  | 'allowsMultipleSelection'
  | 'selectedKeys'
  | 'onSelectionChange'
  | 'leadingSwipeActionsForItem'
  | 'trailingSwipeActionsForItem'
  | 'contextMenuForItem'
  | 'prefetchDataSource'
  | 'ItemSeparatorComponent'
>;

export interface SectionListLocation {
  sectionIndex: number;
  itemIndex: number;
  animated?: boolean;
  viewOffset?: number;
  viewPosition?: number;
}

export interface SectionListCommands extends ShadowListCommands {
  scrollToLocation: (params: SectionListLocation) => void;
  scrollToSection: (sectionIndex: number, animated?: boolean) => void;
}

export interface SectionItemSeparatorProps<ItemT, SectionT = object> {
  highlighted: boolean;
  leadingItem: ItemT | undefined;
  trailingItem: ItemT | undefined;
  section: SectionListData<ItemT, SectionT>;
}

export interface SectionSeparatorProps<ItemT, SectionT = object> {
  highlighted: boolean;
  leadingItem: ItemT | undefined;
  leadingSection: SectionListData<ItemT, SectionT> | undefined;
  section: SectionListData<ItemT, SectionT>;
  trailingItem: ItemT | undefined;
  trailingSection: SectionListData<ItemT, SectionT> | undefined;
}

export interface SectionListProps<ItemT, SectionT = object> extends Omit<
  ShadowListForwardedProps,
  'nonAnchorKeys' | 'persistentKeys'
> {
  sections: ReadonlyArray<SectionListData<ItemT, SectionT>>;
  renderItem?: SectionListRenderItem<ItemT, SectionT>;
  renderSectionHeader?: (info: {
    section: SectionListData<ItemT, SectionT>;
  }) => ReactElement | null;
  renderSectionFooter?: (info: {
    section: SectionListData<ItemT, SectionT>;
  }) => ReactElement | null;
  keyExtractor?: (item: ItemT, index: number) => string;
  stickySectionHeadersEnabled?: boolean;
  sectionIndexTitles?: ReadonlyArray<string>;
  sectionForSectionIndexTitle?: (title: string, index: number) => number;
  ItemSeparatorComponent?:
    | ReactElement
    | ComponentType<SectionItemSeparatorProps<ItemT, SectionT>>
    | null;
  SectionSeparatorComponent?:
    | ReactElement
    | ComponentType<SectionSeparatorProps<ItemT, SectionT>>
    | null;
  getItemSizeSpec?: (
    item: ItemT,
    index: number,
    section: SectionListData<ItemT, SectionT>
  ) => ItemSizeSpec | null | undefined;
  nonAnchorKeys?: ReadonlyArray<string>;
  persistentKeys?: ReadonlyArray<string>;
}

/*
 * TreeList types. Nodes whose parents are all expanded are flattened into one list, and
 * collapsed branches are never walked. keyExtractor must return a key that is unique
 * across the whole tree and stays the same when nodes expand or collapse. Rows and
 * cached sizes then stay matched.
 */
export interface TreeListRenderItemInfo<ItemT> {
  item: ItemT;
  index: number;
  depth: number;
  isExpanded: boolean;
  hasChildren: boolean;
  indent: number;
  toggle: () => void;
  separators: Separators;
}

export interface TreeListCommands extends ShadowListCommands {
  scrollToNode: (key: string, viewPosition?: number) => void;
}

export interface TreeListProps<ItemT> extends ShadowListForwardedProps {
  data: ReadonlyArray<ItemT>;
  getChildren: (item: ItemT) => ReadonlyArray<ItemT> | undefined;
  keyExtractor: (item: ItemT) => string;
  renderItem: (info: TreeListRenderItemInfo<ItemT>) => ReactElement;
  expandedKeys?: ReadonlyArray<string> | ReadonlySet<string>;
  initialExpandedKeys?: ReadonlyArray<string> | ReadonlySet<string>;
  onExpandedChange?: (expandedKeys: Set<string>) => void;
  indentWidth?: number;
  getItemSizeSpec?: (
    item: ItemT,
    index: number,
    depth: number
  ) => ItemSizeSpec | null | undefined;
  ItemSeparatorComponent?:
    | ReactElement
    | ComponentType<ItemSeparatorProps<ItemT>>
    | null;
}
