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

export interface ViewToken<ElementT> {
  item: ElementT;
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

export interface ScrollToItemParams<ElementT = unknown> {
  item: ElementT;
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
  scrollToItem: {
    (index: number, viewPosition?: number, animated?: boolean): void;
    (params: ScrollToItemParams): void;
  };
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
  getElementSize: (key: string) => number | undefined;
  getElementSizes: () => ReadonlyMap<string, number>;
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

export interface ViewableItemsChangedInfo<ElementT> {
  viewableItems: ViewToken<ElementT>[];
  changed: ViewToken<ElementT>[];
}

export interface ViewabilityConfigCallbackPair<ElementT> {
  viewabilityConfig: ViewabilityConfig;
  onViewableItemsChanged:
    | ((info: ViewableItemsChangedInfo<ElementT>) => void)
    | null
    | undefined;
}

/*
 * What a row can do to the separators next to it, like FlatList's separators. leading is the
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

export interface ItemSeparatorProps<ElementT> {
  highlighted: boolean;
  leadingItem: ElementT;
  trailingItem: ElementT | undefined;
}

export interface RenderElementInfo<ElementT> {
  element: ElementT;
  index: number;
  separators: Separators;
  selected: boolean;
  select: () => void;
  deselect: () => void;
}

export interface SwipeAction<ElementT> {
  title: string;
  style?: 'normal' | 'destructive';
  backgroundColor?: ColorValue;
  onPress: (info: { item: ElementT; index: number }) => void | Promise<unknown>;
}

export interface SwipeActionsConfiguration<ElementT> {
  actions: ReadonlyArray<SwipeAction<ElementT>>;
  performsFirstActionWithFullSwipe?: boolean;
}

export interface ContextMenuAction<ElementT> {
  title: string;
  style?: 'normal' | 'destructive';
  disabled?: boolean;
  systemImage?: string;
  onPress: (info: { item: ElementT; index: number }) => void;
}

export interface ContextMenu<ElementT> {
  title?: string;
  actions: ReadonlyArray<ContextMenuAction<ElementT>>;
}

export interface PrefetchDataSource {
  prefetchItems: (indices: number[]) => void;
  cancelPrefetchingForItems?: (indices: number[]) => void;
}

export type ScrollEvent = { nativeEvent: OnScroll };

export interface ElementSizeSpec {
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

export interface ShadowListProps<ElementT extends { id: string }> {
  data: ReadonlyArray<ElementT>;
  renderElement: (info: RenderElementInfo<ElementT>) => ReactElement;
  keyExtractor?: (element: ElementT, index: number) => string;
  style?: ViewStyle;
  elementStyle?: ViewStyle;
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
  onReorder?: (info: { from: number; to: number; data: ElementT[] }) => void;
  stickyIndices?: ReadonlyArray<number>;
  renderStickyHeaderOverlay?: (activeIndex: number) => ReactElement | null;
  numberOfColumns?: number;
  overscan?: number;
  overscanRows?: number;
  overscanRowsLeading?: number;
  getElementSizeSpec?: (
    element: ElementT,
    index: number
  ) => ElementSizeSpec | null | undefined;
  measureLookaheadRows?: number;
  persistentKeys?: ReadonlyArray<string>;
  nonAnchorKeys?: ReadonlyArray<string>;
  initialScrollIndex?: number | null;
  containerOffsetIndex?: number;
  trackElementSizes?: boolean;
  extraData?: unknown;
  refreshing?: boolean;
  onRefresh?: () => void;
  refreshColor?: ColorValue;
  progressViewOffset?: number;
  initialElementsSize?: number;
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
  onViewableItemsChanged?: (info: ViewableItemsChangedInfo<ElementT>) => void;
  viewabilityConfigCallbackPairs?: ReadonlyArray<
    ViewabilityConfigCallbackPair<ElementT>
  >;
  allowsMultipleSelection?: boolean;
  selectedKeys?: ReadonlyArray<string>;
  onSelectionChange?: (selectedKeys: string[]) => void;
  leadingSwipeActionsForItem?: (info: {
    item: ElementT;
    index: number;
  }) => SwipeActionsConfiguration<ElementT> | null | undefined;
  trailingSwipeActionsForItem?: (info: {
    item: ElementT;
    index: number;
  }) => SwipeActionsConfiguration<ElementT> | null | undefined;
  contextMenuForItem?: (info: {
    item: ElementT;
    index: number;
  }) => ContextMenu<ElementT> | null | undefined;
  prefetchDataSource?: PrefetchDataSource;
  prefetchRows?: number;
  ItemSeparatorComponent?:
    | ReactElement
    | ComponentType<ItemSeparatorProps<ElementT>>
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
export interface SectionBase<ElementT, SectionT = object> {
  data: ReadonlyArray<ElementT>;
  key?: string;
  renderElement?: SectionListRenderElement<ElementT, SectionT>;
  keyExtractor?: (element: ElementT, index: number) => string;
}

export type SectionListData<ElementT, SectionT = object> = SectionT &
  SectionBase<ElementT, SectionT>;

export interface SectionListRenderElementInfo<ElementT, SectionT = object> {
  element: ElementT;
  index: number;
  section: SectionListData<ElementT, SectionT>;
  separators: Separators;
}

export type SectionListRenderElement<ElementT, SectionT = object> = (
  info: SectionListRenderElementInfo<ElementT, SectionT>
) => ReactElement | null;

/*
 * ShadowList props that SectionList and TreeList pass through as is. Props that point at
 * rows are left out, since those lists render flattened rows the caller never sees. They
 * define their own version of such a prop or don't offer it.
 */
export type ShadowListForwardedProps = Omit<
  ShadowListProps<{ id: string }>,
  | 'data'
  | 'renderElement'
  | 'keyExtractor'
  | 'getElementSizeSpec'
  | 'stickyIndices'
  | 'renderStickyHeaderOverlay'
  | 'reorderEnabled'
  | 'onReorder'
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

export interface SectionItemSeparatorProps<ElementT, SectionT = object> {
  highlighted: boolean;
  leadingItem: ElementT | undefined;
  trailingItem: ElementT | undefined;
  section: SectionListData<ElementT, SectionT>;
}

export interface SectionSeparatorProps<ElementT, SectionT = object> {
  highlighted: boolean;
  leadingItem: ElementT | undefined;
  leadingSection: SectionListData<ElementT, SectionT> | undefined;
  section: SectionListData<ElementT, SectionT>;
  trailingItem: ElementT | undefined;
  trailingSection: SectionListData<ElementT, SectionT> | undefined;
}

export interface SectionListProps<ElementT, SectionT = object> extends Omit<
  ShadowListForwardedProps,
  'nonAnchorKeys' | 'persistentKeys'
> {
  sections: ReadonlyArray<SectionListData<ElementT, SectionT>>;
  renderElement?: SectionListRenderElement<ElementT, SectionT>;
  renderSectionHeader?: (info: {
    section: SectionListData<ElementT, SectionT>;
  }) => ReactElement | null;
  renderSectionFooter?: (info: {
    section: SectionListData<ElementT, SectionT>;
  }) => ReactElement | null;
  keyExtractor?: (element: ElementT, index: number) => string;
  stickySectionHeadersEnabled?: boolean;
  sectionIndexTitles?: ReadonlyArray<string>;
  sectionForSectionIndexTitle?: (title: string, index: number) => number;
  ItemSeparatorComponent?:
    | ReactElement
    | ComponentType<SectionItemSeparatorProps<ElementT, SectionT>>
    | null;
  SectionSeparatorComponent?:
    | ReactElement
    | ComponentType<SectionSeparatorProps<ElementT, SectionT>>
    | null;
  getElementSizeSpec?: (
    element: ElementT,
    index: number,
    section: SectionListData<ElementT, SectionT>
  ) => ElementSizeSpec | null | undefined;
  nonAnchorKeys?: ReadonlyArray<string>;
  persistentKeys?: ReadonlyArray<string>;
}

/*
 * TreeList types. Nodes whose parents are all expanded are flattened into one list, and
 * collapsed branches are never walked. keyExtractor must return an id that is unique
 * across the whole tree and stays the same when nodes expand or collapse. Rows and
 * cached sizes then stay matched.
 */
export interface TreeListRenderElementInfo<ElementT> {
  element: ElementT;
  index: number;
  depth: number;
  isExpanded: boolean;
  hasChildren: boolean;
  indent: number;
  toggle: () => void;
  separators: Separators;
}

export interface TreeListCommands extends ShadowListCommands {
  scrollToNode: (id: string, viewPosition?: number) => void;
}

export interface TreeListProps<ElementT> extends ShadowListForwardedProps {
  data: ReadonlyArray<ElementT>;
  getChildren: (element: ElementT) => ReadonlyArray<ElementT> | undefined;
  keyExtractor: (element: ElementT) => string;
  renderElement: (info: TreeListRenderElementInfo<ElementT>) => ReactElement;
  expandedIds?: ReadonlyArray<string> | ReadonlySet<string>;
  initialExpandedIds?: ReadonlyArray<string> | ReadonlySet<string>;
  onExpandedChange?: (expandedIds: Set<string>) => void;
  indentWidth?: number;
  getElementSizeSpec?: (
    element: ElementT,
    index: number,
    depth: number
  ) => ElementSizeSpec | null | undefined;
  ItemSeparatorComponent?:
    | ReactElement
    | ComponentType<ItemSeparatorProps<ElementT>>
    | null;
}
