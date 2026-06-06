import type { ReactElement } from 'react';
import type { ViewStyle, ColorValue } from 'react-native';
import type { OnScroll } from './ShadowlistViewNativeComponent';

/*
 * A single item's viewability state.
 */
export interface ViewToken<ElementT> {
  item: ElementT;
  index: number;
  key: string;
  isViewable: boolean;
}

/*
 * Imperative handle exposed via ref.
 */
export interface ShadowlistCommands {
  setStartReachedEnabled: (enabled: boolean) => void;
  setEndReachedEnabled: (enabled: boolean) => void;
  scrollToIndex: (index: number) => void;
  scrollToOffset: (offset: number, animated?: boolean) => void;
  scrollToEnd: (animated?: boolean) => void;
}

export interface ViewabilityConfig {
  /*
   * Percent (0..100) of an item that must be visible before it counts as viewable.
   */
  itemVisiblePercentThreshold?: number;
}

/*
 * Public props.
 */
export interface ShadowlistProps<ElementT extends { id: string }> {
  data: ReadonlyArray<ElementT>;
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
  keyExtractor?: (item: ElementT, index: number) => string;
  style?: ViewStyle;
  elementStyle?: ViewStyle;
  inverted?: boolean;
  horizontal?: boolean;
  stickyHeader?: boolean;
  stickyFooter?: boolean;
  /*
   * Auto-hide the header/footer on scroll: pins to its edge, then slides away as you
   * scroll toward the content and slides back the other way.
   */
  autoHideHeader?: boolean;
  autoHideFooter?: boolean;
  /*
   * Enable long-press drag-to-reorder. Reports the final move through onReorder;
   * pair with a persisted setData there, otherwise the list snaps back on drop.
   */
  dragEnabled?: boolean;
  /*
   * Called once when a drag-to-reorder gesture is released. `data` is the reordered
   * array (set it back into your state); `from`/`to` are the original/final indices.
   */
  onReorder?: (info: { from: number; to: number; data: ElementT[] }) => void;
  /*
   * Element indices that are sticky section headers (ascending). Used by SectionList.
   */
  stickyHeaderIndices?: ReadonlyArray<number>;
  /*
   * Renders the sticky-header overlay for the active section (by flat element index).
   * Paired with stickyHeaderIndices.
   */
  renderStickyHeaderOverlay?: (activeIndex: number) => ReactElement | null;
  columns?: number;
  /*
   * Declarative scroll-to-index, doubling as the initial scroll position. A value
   * >= 0 opens the list with that index anchored to the viewport start; fires only
   * when the value changes. A negative value is inactive (default -2 = top, or bottom
   * when inverted). The imperative scrollToIndex command takes precedence.
   */
  containerOffsetIndex?: number;
  /*
   * Keyboard avoidance: as the keyboard opens, grow the bottom inset and slide content
   * up by the keyboard overlap so covered rows stay visible. Vertical lists only.
   */
  keyboardAvoidingEnabled?: boolean;
  /*
   * Pixels subtracted from the keyboard overlap, to discount UI already above the
   * keyboard (a tab bar) or safe-area inset below the list. Defaults to 0.
   */
  keyboardAvoidingOffset?: number;
  /*
   * Pull-to-refresh (vertical lists only). Provide onRefresh to enable; drive the
   * indicator with the controlled `refreshing` flag.
   */
  refreshing?: boolean;
  onRefresh?: () => void;
  /*
   * Tint for the pull-to-refresh indicator. Defaults to the platform default.
   */
  refreshColor?: ColorValue;
  initialElementsSize?: number;
  onStartReached?: () => void;
  onEndReached?: () => void;
  /*
   * Distance from the start/end, as a fraction of the visible length, at which the
   * matching callback fires (FlatList semantics). Defaults to 1.
   */
  onStartReachedThreshold?: number;
  onEndReachedThreshold?: number;
  onScroll?: (event: { nativeEvent: OnScroll }) => void;
  viewabilityConfig?: ViewabilityConfig;
  onViewableItemsChanged?: (info: {
    viewableItems: ViewToken<ElementT>[];
    changed: ViewToken<ElementT>[];
  }) => void;
  ItemSeparatorComponent?: ReactElement | (() => ReactElement | null) | null;
  ListHeaderComponent?: ReactElement | (() => ReactElement | null) | null;
  ListFooterComponent?: ReactElement | (() => ReactElement | null) | null;
  ListEmptyComponent?: ReactElement | (() => ReactElement | null) | null;
}

/*
 * SectionList types. A section is `data` plus any caller fields (SectionT),
 * addressed by an optional stable `key`.
 */
export interface SectionBase<ItemT, SectionT = object> {
  data: ReadonlyArray<ItemT>;
  key?: string;
  /*
   * Per-section overrides; fall back to the top-level SectionList props.
   */
  renderItem?: SectionListRenderItem<ItemT, SectionT>;
  keyExtractor?: (item: ItemT, index: number) => string;
}

export type SectionListData<ItemT, SectionT = object> = SectionT &
  SectionBase<ItemT, SectionT>;

export interface SectionListRenderItemInfo<ItemT, SectionT = object> {
  item: ItemT;
  /*
   * The item's index within its section (not the flattened list).
   */
  index: number;
  section: SectionListData<ItemT, SectionT>;
}

export type SectionListRenderItem<ItemT, SectionT = object> = (
  info: SectionListRenderItemInfo<ItemT, SectionT>
) => ReactElement | null;

export interface SectionListProps<ItemT, SectionT = object> {
  sections: ReadonlyArray<SectionListData<ItemT, SectionT>>;
  renderItem?: SectionListRenderItem<ItemT, SectionT>;
  renderSectionHeader?: (info: {
    section: SectionListData<ItemT, SectionT>;
  }) => ReactElement | null;
  renderSectionFooter?: (info: {
    section: SectionListData<ItemT, SectionT>;
  }) => ReactElement | null;
  keyExtractor?: (item: ItemT, index: number) => string;
  /*
   * Pin section headers to the viewport top, swapping as the next section arrives.
   * Defaults to true.
   */
  stickySectionHeadersEnabled?: boolean;
  /*
   * Rendered between items within a section (not after the last item).
   */
  ItemSeparatorComponent?: ReactElement | (() => ReactElement | null) | null;
  /*
   * Rendered between sections (after a section's last row, before the next header).
   */
  SectionSeparatorComponent?: ReactElement | (() => ReactElement | null) | null;
  style?: ViewStyle;
  elementStyle?: ViewStyle;
  inverted?: boolean;
  initialElementsSize?: number;
  containerOffsetIndex?: number;
  /* See ShadowlistProps.keyboardAvoidingEnabled. */
  keyboardAvoidingEnabled?: boolean;
  /* See ShadowlistProps.keyboardAvoidingOffset. */
  keyboardAvoidingOffset?: number;
  /* See ShadowlistProps.refreshing / onRefresh. */
  refreshing?: boolean;
  onRefresh?: () => void;
  /* See ShadowlistProps.refreshColor. */
  refreshColor?: ColorValue;
  onScroll?: (event: { nativeEvent: OnScroll }) => void;
  onStartReached?: () => void;
  onEndReached?: () => void;
  onStartReachedThreshold?: number;
  onEndReachedThreshold?: number;
  ListHeaderComponent?: ReactElement | (() => ReactElement | null) | null;
  ListFooterComponent?: ReactElement | (() => ReactElement | null) | null;
  ListEmptyComponent?: ReactElement | (() => ReactElement | null) | null;
}

/*
 * TreeList types. The visible subtree (nodes whose ancestors are all expanded) is
 * flattened into one element stream; collapsed subtrees are never walked.
 * `keyExtractor` must return a globally unique, stable id per node (stable across
 * expand/collapse) so reconcile and measurement caching line up.
 */
export interface TreeListRenderNodeInfo<ItemT> {
  item: ItemT;
  /*
   * Index of the node in the flattened visible stream (not within its parent).
   */
  index: number;
  /*
   * Nesting level; 0 for roots.
   */
  depth: number;
  isExpanded: boolean;
  hasChildren: boolean;
  /*
   * depth * indentWidth, the leading inset in px.
   */
  indent: number;
  /*
   * Toggle this node's expanded state. No-op for leaves.
   */
  toggle: () => void;
}

/*
 * Imperative handle for TreeList: the standard Shadowlist commands plus
 * scrollToNode (no-op when the node is not in the visible/expanded set).
 */
export interface TreeListCommands extends ShadowlistCommands {
  scrollToNode: (id: string) => void;
}

export interface TreeListProps<ItemT> {
  /*
   * Root nodes. Children are reached through getChildren, recursively.
   */
  data: ReadonlyArray<ItemT>;
  /*
   * Return a node's children, or undefined/empty for a leaf. Called only for visited
   * nodes, so a lazy-loading implementation can fetch on demand.
   */
  getChildren: (item: ItemT) => ReadonlyArray<ItemT> | undefined;
  /*
   * Globally unique, stable id per node. Must be stable across expand/collapse.
   */
  keyExtractor: (item: ItemT) => string;
  renderNode: (info: TreeListRenderNodeInfo<ItemT>) => ReactElement;
  /*
   * Controlled expansion: the set of expanded node ids. When provided, the list
   * does not own expansion state and reports intended changes through
   * onExpandedChange. Omit for uncontrolled mode (see initialExpandedIds).
   */
  expandedIds?: ReadonlyArray<string> | ReadonlySet<string>;
  /*
   * Uncontrolled mode: ids expanded on first render. Ignored when expandedIds is
   * provided.
   */
  initialExpandedIds?: ReadonlyArray<string> | ReadonlySet<string>;
  /*
   * Fires with the next expanded set whenever a node is toggled. Required to
   * persist state in controlled mode; optional notification in uncontrolled mode.
   */
  onExpandedChange?: (expandedIds: Set<string>) => void;
  /*
   * Pixels of leading inset per depth level. Default 16.
   */
  indentWidth?: number;
  style?: ViewStyle;
  elementStyle?: ViewStyle;
  initialElementsSize?: number;
  containerOffsetIndex?: number;
  /* See ShadowlistProps.keyboardAvoidingEnabled. */
  keyboardAvoidingEnabled?: boolean;
  /* See ShadowlistProps.keyboardAvoidingOffset. */
  keyboardAvoidingOffset?: number;
  /* See ShadowlistProps.refreshing / onRefresh. */
  refreshing?: boolean;
  onRefresh?: () => void;
  /* See ShadowlistProps.refreshColor. */
  refreshColor?: ColorValue;
  onScroll?: (event: { nativeEvent: OnScroll }) => void;
  onStartReached?: () => void;
  onEndReached?: () => void;
  onStartReachedThreshold?: number;
  onEndReachedThreshold?: number;
  ItemSeparatorComponent?: ReactElement | (() => ReactElement | null) | null;
  ListHeaderComponent?: ReactElement | (() => ReactElement | null) | null;
  ListFooterComponent?: ReactElement | (() => ReactElement | null) | null;
  ListEmptyComponent?: ReactElement | (() => ReactElement | null) | null;
}
