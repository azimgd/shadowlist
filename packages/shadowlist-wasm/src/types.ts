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
 * A single item's viewability state, mirroring FlatList's ViewToken shape.
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
 * Public props. Kept aligned with the native package so application code is
 * portable across platforms.
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
  columns?: number;

  /*
   * Element indices that are sticky section headers (ascending). Used by SectionList
   * to pin section headers; a plain list leaves this undefined.
   */
  stickyHeaderIndices?: ReadonlyArray<number>;
  /*
   * Renders the content of the always-mounted sticky-header overlay for the active
   * section (identified by its flat element index). Paired with stickyHeaderIndices;
   * the overlay is pinned to the viewport on every scroll frame, so it never lags.
   */
  renderStickyHeaderOverlay?: (activeIndex: number) => ReactNode;

  /*
   * Enables long-press drag-to-reorder. The pickup, pointer tracking, edge auto-
   * scroll and make-room shuffle are all handled by imperative transforms (no
   * re-render while dragging); the final move is reported once through onReorder.
   * Pair with a persisted setData in onReorder, otherwise the list snaps back.
   */
  dragEnabled?: boolean;
  /*
   * Called once when a drag-to-reorder gesture is released. `data` is the fully
   * reordered array (set it back into your state); `from`/`to` are the picked-up
   * element's original and final indices.
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

  ItemSeparatorComponent?: ListComponent;
  ListHeaderComponent?: ListComponent;
  ListFooterComponent?: ListComponent;
  ListEmptyComponent?: ListComponent;
}

/*
 * SectionList types, mirroring React Native's SectionList (and the native package)
 * so application code ports across platforms. A section is `data` plus any caller
 * fields (SectionT), addressed by an optional stable `key`.
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
) => ReactNode;

export interface SectionListProps<ItemT, SectionT = object> {
  sections: ReadonlyArray<SectionListData<ItemT, SectionT>>;
  renderItem?: SectionListRenderItem<ItemT, SectionT>;
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
 * TreeList types. A directory-browser / outline tree built as a thin data layer
 * over Shadowlist: the visible subtree (every node whose ancestors are all
 * expanded) is flattened into one element stream and handed to the virtualizer,
 * exactly how SectionList flattens sections. Collapsed subtrees are never walked,
 * so flatten cost scales with the number of *visible* rows, not the total tree.
 * Expand/collapse only changes the flat key set, so the engine's key-based
 * reconcile keeps measured row sizes and the toggled row stays anchored.
 *
 * Nodes are caller-defined (ItemT); the tree is described by two accessors rather
 * than a fixed wrapper shape. `keyExtractor` must return a globally unique, stable
 * id per node so reconcile and measurement caching line up.
 */
export interface TreeListRenderNodeInfo<ItemT> {
  item: ItemT;
  /*
   * Index of the node in the flattened visible stream (not within its parent).
   */
  index: number;
  /*
   * Nesting level; 0 for roots. Multiply by indentWidth for the leading inset
   * (also provided pre-computed as `indent`).
   */
  depth: number;
  isExpanded: boolean;
  hasChildren: boolean;
  /*
   * Convenience: depth * indentWidth, the leading inset in px.
   */
  indent: number;
  /*
   * Toggle this node's expanded state. No-op for leaves. Honours the
   * controlled/uncontrolled mode the list is in.
   */
  toggle: () => void;
}

/*
 * Imperative handle for TreeList: the standard Shadowlist commands plus
 * scrollToNode, which resolves a node id to its current flat index and scrolls to
 * it (no-op when the node is not in the visible/expanded set).
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
   * nodes that are actually visited (visible nodes and the children of expanded
   * ones), so a lazy-loading implementation can fetch on demand.
   */
  getChildren: (item: ItemT) => ReadonlyArray<ItemT> | undefined;
  /*
   * Globally unique, stable id per node. Must be stable across expand/collapse.
   */
  keyExtractor: (item: ItemT) => string;
  renderNode: (info: TreeListRenderNodeInfo<ItemT>) => ReactNode;
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
