import type { ReactElement } from 'react';
import type { ViewStyle } from 'react-native';
import type { OnScroll } from './ShadowlistViewNativeComponent';

/*
 * A single item's viewability state, mirroring FlatList's ViewToken shape so the
 * onViewableItemsChanged contract is familiar.
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
 * Public props. Kept aligned with the web (wasm) package so application code is
 * portable across platforms.
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
   * Auto-hide the header/footer on scroll: the bar pins to its edge, then slides away
   * as you scroll toward the content (down for the header, down for the footer) and
   * slides back as you scroll the other way. Direction-based, handled natively. The
   * header stays shown near the top and the footer near the bottom.
   */
  autoHideHeader?: boolean;
  autoHideFooter?: boolean;
  /*
   * Enables native long-press drag-to-reorder. The pickup, finger tracking, edge
   * auto-scroll and live shuffle run entirely natively; the component mirrors the
   * resulting order and reports the final move through onReorder. Pair with a
   * persisted setData in onReorder, otherwise the list snaps back on drop.
   */
  dragEnabled?: boolean;
  /*
   * Called once when a drag-to-reorder gesture is released. `data` is the fully
   * reordered array (the authoritative result - set it back into your state);
   * `from`/`to` are the picked-up element's original and final indices.
   */
  onReorder?: (info: { from: number; to: number; data: ElementT[] }) => void;
  /*
   * Element indices that are sticky section headers (ascending). Used by SectionList
   * to pin section headers natively; a plain list leaves this undefined.
   */
  stickyHeaderIndices?: ReadonlyArray<number>;
  /*
   * Renders the content of the always-mounted sticky-header overlay for the active
   * section (identified by its flat element index). Paired with stickyHeaderIndices;
   * the overlay is pinned to the viewport natively, so its position never lags.
   */
  renderStickyHeaderOverlay?: (activeIndex: number) => ReactElement | null;
  columns?: number;
  containerOffsetIndex?: number;
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
 * SectionList types, mirroring React Native's SectionList so application code and
 * AI conversions port across with minimal changes. A section is `data` plus any
 * caller fields (SectionT), addressed by an optional stable `key`.
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
   * Pin section headers to the viewport top as their section scrolls under them,
   * swapping as the next section arrives. Pinned natively. Defaults to true.
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
 * TreeList types. A directory-browser / outline tree built as a thin data layer
 * over Shadowlist: the visible subtree (every node whose ancestors are all
 * expanded) is flattened into one element stream and handed to the virtualizer,
 * exactly how SectionList flattens sections. Collapsed subtrees are never walked,
 * so flatten cost scales with the number of *visible* rows, not the total tree.
 * Expand/collapse only changes the flat key set, so the engine's key-based
 * reconcile keeps measured row sizes and maintain-visible-content-position keeps
 * the toggled row anchored - sub-millisecond toggles on trees of any size.
 *
 * Nodes are caller-defined (ItemT); the tree is described by two accessors rather
 * than a fixed wrapper shape, so no per-node objects are allocated for the source
 * data. `keyExtractor` must return a globally unique, stable id per node (the same
 * node must keep its key across expand/collapse) so reconcile and measurement
 * caching line up.
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
