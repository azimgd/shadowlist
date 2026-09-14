import type { ReactElement } from 'react';
import type {
  ViewStyle,
  TextStyle,
  ColorValue,
  AccessibilityRole,
} from 'react-native';
import type { OnScroll } from './ShadowListViewNativeComponent';

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
export interface ShadowListCommands {
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
 * What a row will measure to, described before it is rendered.
 *
 * Returned from ShadowList's `getElementSizeSpec`. Native measures the text through the same
 * engine the real layout pass uses, so the height is the one the row would have got anyway
 * -- just computed a few frames earlier, off the commit path.
 *
 * Describe the text and declare everything around it as insets. A row whose height is not a
 * function of its text (an inline image, async content) should return null instead of a
 * guess: the core estimates and then measures those rows normally.
 */
export interface ElementSizeSpec {
  /*
   * The row's text. Concatenate the pieces if a row has several lines of the same style;
   * use `insetHeight` for anything styled differently.
   */
  text: string;

  /*
   * Text attributes that affect wrapping. These must match the styles the row actually
   * renders with, or the prediction will be confidently wrong -- read them from the same
   * StyleSheet the row uses rather than retyping the numbers.
   */
  fontSize?: number;
  fontFamily?: string;
  fontWeight?: TextStyle['fontWeight'];
  fontStyle?: 'normal' | 'italic';
  lineHeight?: number;
  letterSpacing?: number;
  /*
   * Same meaning as <Text numberOfLines>: truncate after this many lines. 0 (the default)
   * means no limit.
   */
  numberOfLines?: number;

  /*
   * Everything in the row that is not this text, in points.
   *
   * `insetWidth` is horizontal space the text cannot use (row padding, an avatar column, a
   * trailing icon) and narrows what it wraps within. `insetHeight` is vertical space added
   * around it (padding, borders, a fixed header or footer strip inside the row).
   */
  insetWidth?: number;
  insetHeight?: number;

  /*
   * Fraction of the list width the text may occupy, applied before `insetWidth`. For a row
   * element with a percentage width -- a chat bubble capped at `maxWidth: '75%'` -- which no
   * fixed inset can express. Defaults to 1.
   */
  widthFraction?: number;

  /*
   * A height you already know exactly. Set it and no text measurement happens at all --
   * the cheapest prediction available, and the right one for fixed-height rows, separators
   * and spacers. `text` is ignored.
   */
  fixedHeight?: number;
}

/*
 * Public props.
 */
export interface ShadowListProps<ElementT extends { id: string }> {
  data: ReadonlyArray<ElementT>;
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
  keyExtractor?: (element: ElementT, index: number) => string;
  style?: ViewStyle;
  elementStyle?: ViewStyle;
  /*
   * Standard RN accessibility props for the list's own container view -- the same
   * ones any RN <View> accepts, passed through as-is rather than a bespoke
   * accessibility API. They describe the list container as a whole, not its rows: give
   * each row its own accessibilityLabel/accessibilityRole etc. from inside
   * `renderElement` instead (ShadowList doesn't add per-row accessibility props of its
   * own since renderElement already returns an arbitrary element you fully control).
   */
  accessible?: boolean;
  accessibilityLabel?: string;
  accessibilityRole?: AccessibilityRole;
  accessibilityHint?: string;
  testID?: string;
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
   * `DraggableList` is `ShadowList` with this defaulted to `true`.
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
   * How far beyond the visible area to render rows, in viewport units. 1 (the default)
   * keeps one viewport of rows mounted above and one below, so scrolling reveals ready
   * rows instead of blanks. Raise it for smoother fast scrolling at the cost of more
   * mounted rows; lower it toward 0 to mount fewer.
   *
   * See nativeViewOverscan: raising overscan alone also raises the number of native
   * views, which is the cost that is paid on every frame rather than only while scrolling.
   */
  overscan?: number;
  /*
   * How far beyond the visible area rows exist as NATIVE VIEWS, in viewport units.
   *
   * overscan decides how far out rows stay rendered, which is what keeps a fast scroll from
   * waiting on the JS round trip and showing blanks. This decides how many of those rendered
   * rows are actually materialized. Rows in between keep their measured geometry and their
   * React state, but their contents are dropped from the view tree, so the UI thread stops
   * laying them out, hit-testing them and walking them for accessibility.
   *
   * That split is what lets overscan go up (smooth scrolling) while the live view count goes
   * down. A row is promoted back natively, without a re-render, as soon as it re-enters this
   * band. Rows that have never been measured are never dropped. A focused TextInput or
   * accessibility (VoiceOver/TalkBack) focus inside a row that leaves the band loses focus.
   *
   * Undefined (the default) keeps the two bands identical, i.e. the behaviour before this
   * option existed. Try 0.5 with an overscan of 3.
   *
   * Platforms: iOS and macOS. On Android it has no effect unless the app enables React
   * Native's `useTraitHiddenOnAndroid` feature flag, without which RN still mounts hidden
   * rows. Promotion happens in a native commit that runs on the JS thread, so the band widens
   * while the list is dragged or decelerating, and rows are never hidden while a
   * drag-to-reorder gesture is active.
   */
  nativeViewOverscan?: number;
  /*
   * Describe a row's size before it is rendered, so native can compute its real height
   * ahead of time instead of guessing and then correcting.
   *
   * Without this the core estimates every row, learns the truth only when React renders it
   * and Yoga lays it out, and reflows everything below -- continuously, for the whole
   * scroll, on a variable-height list. Each of those reflows can also nudge the scroll
   * offset to keep the anchored row in place. With it, geometry is right from the first
   * frame: the scrollbar is honest, `scrollToIndex` lands immediately, and rows can be
   * dematerialized before they have ever been mounted.
   *
   * Called only for rows near the viewport, not for the whole dataset. Return null for any
   * row you cannot describe exactly; those fall back to the ordinary estimate, and mixing
   * the two is expected. Keep the function referentially stable (useCallback) -- it is a
   * memo dependency.
   *
   * Worth most on long variable-height text lists, such as chat. A fixed-height list
   * already has exact geometry and gains nothing.
   */
  getElementSizeSpec?: (
    element: ElementT,
    index: number
  ) => ElementSizeSpec | null | undefined;
  /*
   * How many rows beyond the mounted window to describe, so predictions are in place before
   * a scroll reaches those rows.
   *
   * Also sets how often the prop is republished, which is the expensive part: a republish
   * makes Fabric deep-copy the whole key vector and costs the core a full key revalidation.
   * The span is re-cut only when the mounted window comes within ~24 rows of its edge, so a
   * larger lookahead means proportionally fewer republishes. Ignored without
   * `getElementSizeSpec`.
   */
  measureLookaheadRows?: number;
  /*
   * Row keys (matching keyExtractor) to keep mounted at all times, even when scrolled
   * far outside the virtualization window. The rows stay at their natural position in
   * the list flow (unlike stickyHeaderIndices, which pin to the viewport edge). Keyed
   * rather than indexed so a pinned row keeps its identity across inserts/removes. Keep
   * the set small: each entry is a permanently live view.
   */
  persistentKeys?: ReadonlyArray<string>;
  /*
   * Row keys (matching keyExtractor) that may never become the scroll anchor. The core keeps
   * the anchor row fixed on screen as content above or around it changes; rows listed here
   * are skipped when it picks one, so a churning decoration row (date pill, typing
   * indicator, trailing spacer) can't perturb the position.
   *
   * An inverted list resting at its bottom follows rows appended below the newest one
   * without this. It is how to follow a row growing in place (a streaming reply): when the
   * only anchorable row near the viewport is the last one, the core pins to the true bottom
   * as that row grows, in the same pipeline that measures it -- no JS scroll commands. Keep
   * the list short and referentially stable; it is compared on every update.
   */
  nonAnchorKeys?: ReadonlyArray<string>;
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
  /*
   * Fire when the viewport nears the first / last element of `data`. The edges follow
   * the data order in every orientation: `inverted` pins the resting position to the end
   * but does not reverse the list, so on an inverted chat (newest last) onStartReached
   * is the "load earlier" edge at the top and onEndReached fires at the bottom.
   */
  onStartReached?: () => void;
  onEndReached?: () => void;
  /*
   * Distance from the start/end, as a fraction of the visible length, at which the
   * matching callback fires (FlatList semantics). Defaults to 1.
   */
  onStartReachedThreshold?: number;
  onEndReachedThreshold?: number;
  onScroll?: (event: { nativeEvent: OnScroll }) => void;
  /*
   * Snap the resting scroll position to an element boundary. Combine with
   * full-viewport elements for fullscreen paging, or smaller elements for
   * multi-item snapping. Works on both axes.
   */
  snapToItem?: boolean;
  /*
   * Which element edge aligns to the viewport when snapping. Default 'start'.
   */
  snapToAlignment?: 'start' | 'center' | 'end';
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
export interface SectionBase<ElementT, SectionT = object> {
  data: ReadonlyArray<ElementT>;
  key?: string;
  /*
   * Per-section overrides; fall back to the top-level SectionList props.
   */
  renderElement?: SectionListRenderElement<ElementT, SectionT>;
  keyExtractor?: (element: ElementT, index: number) => string;
}

export type SectionListData<ElementT, SectionT = object> = SectionT &
  SectionBase<ElementT, SectionT>;

export interface SectionListRenderElementInfo<ElementT, SectionT = object> {
  element: ElementT;
  /*
   * The element's index within its section (not the flattened list).
   */
  index: number;
  section: SectionListData<ElementT, SectionT>;
}

export type SectionListRenderElement<ElementT, SectionT = object> = (
  info: SectionListRenderElementInfo<ElementT, SectionT>
) => ReactElement | null;

export interface SectionListProps<ElementT, SectionT = object> {
  sections: ReadonlyArray<SectionListData<ElementT, SectionT>>;
  renderElement?: SectionListRenderElement<ElementT, SectionT>;
  renderSectionHeader?: (info: {
    section: SectionListData<ElementT, SectionT>;
  }) => ReactElement | null;
  renderSectionFooter?: (info: {
    section: SectionListData<ElementT, SectionT>;
  }) => ReactElement | null;
  keyExtractor?: (element: ElementT, index: number) => string;
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
  /* See ShadowListProps.accessible / accessibilityLabel / accessibilityRole / accessibilityHint / testID. */
  accessible?: boolean;
  accessibilityLabel?: string;
  accessibilityRole?: AccessibilityRole;
  accessibilityHint?: string;
  testID?: string;
  inverted?: boolean;
  initialElementsSize?: number;
  containerOffsetIndex?: number;
  /* See ShadowListProps.overscan. */
  overscan?: number;
  /* See ShadowListProps.nativeViewOverscan. */
  nativeViewOverscan?: number;
  /*
   * See ShadowListProps.getElementSizeSpec. Called for element rows only, with the element's
   * index within its section; section headers and footers are estimated and measured
   * natively.
   */
  getElementSizeSpec?: (
    element: ElementT,
    index: number,
    section: SectionListData<ElementT, SectionT>
  ) => ElementSizeSpec | null | undefined;
  /* See ShadowListProps.measureLookaheadRows. Counts headers and footers as rows. */
  measureLookaheadRows?: number;
  /* See ShadowListProps.nonAnchorKeys. Element keys, as returned by keyExtractor. */
  nonAnchorKeys?: ReadonlyArray<string>;
  /* See ShadowListProps.keyboardAvoidingEnabled. */
  keyboardAvoidingEnabled?: boolean;
  /* See ShadowListProps.keyboardAvoidingOffset. */
  keyboardAvoidingOffset?: number;
  /* See ShadowListProps.refreshing / onRefresh. */
  refreshing?: boolean;
  onRefresh?: () => void;
  /* See ShadowListProps.refreshColor. */
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
export interface TreeListRenderElementInfo<ElementT> {
  element: ElementT;
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
 * Imperative handle for TreeList: the standard ShadowList commands plus
 * scrollToNode (no-op when the node is not in the visible/expanded set).
 */
export interface TreeListCommands extends ShadowListCommands {
  scrollToNode: (id: string) => void;
}

export interface TreeListProps<ElementT> {
  /*
   * Root nodes. Children are reached through getChildren, recursively.
   */
  data: ReadonlyArray<ElementT>;
  /*
   * Return a node's children, or undefined/empty for a leaf. Called only for visited
   * nodes, so a lazy-loading implementation can fetch on demand.
   */
  getChildren: (element: ElementT) => ReadonlyArray<ElementT> | undefined;
  /*
   * Globally unique, stable id per node. Must be stable across expand/collapse.
   */
  keyExtractor: (element: ElementT) => string;
  renderElement: (info: TreeListRenderElementInfo<ElementT>) => ReactElement;
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
  /* See ShadowListProps.accessible / accessibilityLabel / accessibilityRole / accessibilityHint / testID. */
  accessible?: boolean;
  accessibilityLabel?: string;
  accessibilityRole?: AccessibilityRole;
  accessibilityHint?: string;
  testID?: string;
  initialElementsSize?: number;
  containerOffsetIndex?: number;
  /* See ShadowListProps.overscan. */
  overscan?: number;
  /* See ShadowListProps.nativeViewOverscan. */
  nativeViewOverscan?: number;
  /*
   * See ShadowListProps.getElementSizeSpec. `index` is the node's position in the flattened
   * visible stream; `depth` is its nesting level, so the indent can be declared as
   * `insetWidth`.
   */
  getElementSizeSpec?: (
    element: ElementT,
    index: number,
    depth: number
  ) => ElementSizeSpec | null | undefined;
  /* See ShadowListProps.measureLookaheadRows. */
  measureLookaheadRows?: number;
  /* See ShadowListProps.nonAnchorKeys. Node ids, as returned by keyExtractor. */
  nonAnchorKeys?: ReadonlyArray<string>;
  /* See ShadowListProps.keyboardAvoidingEnabled. */
  keyboardAvoidingEnabled?: boolean;
  /* See ShadowListProps.keyboardAvoidingOffset. */
  keyboardAvoidingOffset?: number;
  /* See ShadowListProps.refreshing / onRefresh. */
  refreshing?: boolean;
  onRefresh?: () => void;
  /* See ShadowListProps.refreshColor. */
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
