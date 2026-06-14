import type { CSSProperties, ReactElement, ReactNode } from 'react';

/*
 * Scroll offset payload delivered to the onScroll callback.
 */
export interface OnScroll {
  contentOffsetX: number;
  contentOffsetY: number;
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

/*
 * A single item's viewability state.
 */
export interface ViewToken<ElementT> {
  item: ElementT;
  index: number;
  key: string;
  isViewable: boolean;
}

export interface ViewabilityConfig {
  /*
   * Percent (0..100) of an item that must be visible before it counts as viewable.
   */
  itemVisiblePercentThreshold?: number;
}

type ListComponent = ReactElement | (() => ReactElement | null) | null;

/*
 * Public props.
 */
export interface ShadowlistProps<ElementT extends { id: string }> {
  data: ReadonlyArray<ElementT>;
  renderElement: (info: { element: ElementT; index: number }) => ReactNode;
  keyExtractor?: (item: ElementT, index: number) => string;

  style?: CSSProperties;
  elementStyle?: CSSProperties;

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
  columns?: number;

  /*
   * Element indices that are sticky section headers (ascending).
   */
  stickyHeaderIndices?: ReadonlyArray<number>;
  /*
   * Renders the sticky-header overlay for the active section (by flat element index).
   * Paired with stickyHeaderIndices.
   */
  renderStickyHeaderOverlay?: (activeIndex: number) => ReactNode;

  /*
   * Enables long-press drag-to-reorder. Persist the reordered data in onReorder,
   * otherwise the list snaps back.
   */
  dragEnabled?: boolean;
  /*
   * Called once when a drag-to-reorder gesture is released. `data` is the reordered
   * array; `from`/`to` are the element's original and final indices.
   */
  onReorder?: (info: { from: number; to: number; data: ElementT[] }) => void;

  /*
   * Declarative scroll-to-index. A negative value (the default) is inactive.
   */
  containerOffsetIndex?: number;

  /*
   * How many elements to mount before the first measurement pass.
   */
  initialElementsSize?: number;

  /*
   * Estimated element size (cross-axis, main-axis) for unmeasured elements.
   */
  estimatedElementWidth?: number;
  estimatedElementHeight?: number;

  onStartReached?: () => void;
  onEndReached?: () => void;
  /*
   * Distance from the start/end, as a fraction of the visible length, at which the
   * matching callback fires. Defaults to 1.
   */
  onStartReachedThreshold?: number;
  onEndReachedThreshold?: number;
  onScroll?: (event: { nativeEvent: OnScroll }) => void;

  /*
   * Snap the resting scroll position to an element boundary. Combine with
   * full-viewport elements for fullscreen paging, or smaller elements for
   * multi-item snapping. Implemented with native CSS scroll-snap.
   */
  snapToItem?: boolean;
  /*
   * Which element edge aligns to the viewport when snapping. Default 'start'.
   */
  snapToAlignment?: 'start' | 'center' | 'end';

  /*
   * Pull-to-refresh (non-inverted vertical lists, touch input). Provide onRefresh
   * to enable; drive the spinner with the controlled `refreshing` flag.
   */
  refreshing?: boolean;
  onRefresh?: () => void;
  /*
   * Tint for the pull-to-refresh spinner. Defaults to a neutral gray.
   */
  refreshColor?: string;

  viewabilityConfig?: ViewabilityConfig;
  onViewableItemsChanged?: (info: {
    viewableItems: ViewToken<ElementT>[];
    changed: ViewToken<ElementT>[];
  }) => void;

  ItemSeparatorComponent?: ListComponent;
  ListHeaderComponent?: ListComponent;
  ListFooterComponent?: ListComponent;
  ListEmptyComponent?: ListComponent;
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
  renderElement?: SectionListRenderElement<ItemT, SectionT>;
  keyExtractor?: (item: ItemT, index: number) => string;
}

export type SectionListData<ItemT, SectionT = object> = SectionT &
  SectionBase<ItemT, SectionT>;

export interface SectionListRenderElementInfo<ItemT, SectionT = object> {
  element: ItemT;
  /*
   * The element's index within its section (not the flattened list).
   */
  index: number;
  section: SectionListData<ItemT, SectionT>;
}

export type SectionListRenderElement<ItemT, SectionT = object> = (
  info: SectionListRenderElementInfo<ItemT, SectionT>
) => ReactNode;

export interface SectionListProps<ItemT, SectionT = object> {
  sections: ReadonlyArray<SectionListData<ItemT, SectionT>>;
  renderElement?: SectionListRenderElement<ItemT, SectionT>;
  renderSectionHeader?: (info: {
    section: SectionListData<ItemT, SectionT>;
  }) => ReactNode;
  renderSectionFooter?: (info: {
    section: SectionListData<ItemT, SectionT>;
  }) => ReactNode;
  keyExtractor?: (item: ItemT, index: number) => string;
  /*
   * Pin section headers to the viewport top as their section scrolls under them,
   * swapping as the next section arrives. Defaults to true.
   */
  stickySectionHeadersEnabled?: boolean;
  /*
   * Rendered between items within a section (not after the last item).
   */
  ItemSeparatorComponent?: ListComponent;
  /*
   * Rendered between sections (after a section's last row, before the next header).
   */
  SectionSeparatorComponent?: ListComponent;
  style?: CSSProperties;
  elementStyle?: CSSProperties;
  inverted?: boolean;
  initialElementsSize?: number;
  containerOffsetIndex?: number;
  onScroll?: (event: { nativeEvent: OnScroll }) => void;
  onStartReached?: () => void;
  onEndReached?: () => void;
  onStartReachedThreshold?: number;
  onEndReachedThreshold?: number;
  ListHeaderComponent?: ListComponent;
  ListFooterComponent?: ListComponent;
  ListEmptyComponent?: ListComponent;
}

/*
 * TreeList types. The visible subtree (nodes whose ancestors are all expanded) is
 * flattened into one element stream; collapsed subtrees are never walked. Nodes are
 * caller-defined (ItemT), described by accessors. `keyExtractor` must return a
 * globally unique, stable id per node.
 */
export interface TreeListRenderElementInfo<ItemT> {
  element: ItemT;
  /*
   * Index of the node in the flattened visible stream (not within its parent).
   */
  index: number;
  /*
   * Nesting level; 0 for roots. Leading inset is provided pre-computed as `indent`.
   */
  depth: number;
  isExpanded: boolean;
  hasChildren: boolean;
  /*
   * Convenience: depth * indentWidth, the leading inset in px.
   */
  indent: number;
  /*
   * Toggle this node's expanded state. No-op for leaves.
   */
  toggle: () => void;
}

/*
 * Imperative handle for TreeList: the standard commands plus scrollToNode, which
 * scrolls to a node by id (no-op when the node is not visible/expanded).
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
   * Return a node's children, or undefined/empty for a leaf. Called only for
   * visited nodes, so a lazy-loading implementation can fetch on demand.
   */
  getChildren: (item: ItemT) => ReadonlyArray<ItemT> | undefined;
  /*
   * Globally unique, stable id per node. Must be stable across expand/collapse.
   */
  keyExtractor: (item: ItemT) => string;
  renderElement: (info: TreeListRenderElementInfo<ItemT>) => ReactNode;
  /*
   * Controlled expansion: the set of expanded node ids. When provided, the list
   * reports intended changes through onExpandedChange. Omit for uncontrolled mode.
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
  style?: CSSProperties;
  elementStyle?: CSSProperties;
  initialElementsSize?: number;
  containerOffsetIndex?: number;
  onScroll?: (event: { nativeEvent: OnScroll }) => void;
  onStartReached?: () => void;
  onEndReached?: () => void;
  onStartReachedThreshold?: number;
  onEndReachedThreshold?: number;
  ItemSeparatorComponent?: ListComponent;
  ListHeaderComponent?: ListComponent;
  ListFooterComponent?: ListComponent;
  ListEmptyComponent?: ListComponent;
}
