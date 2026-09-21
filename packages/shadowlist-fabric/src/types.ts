import type { ReactElement } from 'react';
import type {
  ViewStyle,
  TextStyle,
  ColorValue,
  AccessibilityRole,
} from 'react-native';
import type { OnScroll } from './ShadowListViewNativeComponent';

export interface ViewToken<ElementT> {
  item: ElementT;
  index: number;
  key: string;
  isViewable: boolean;
}

export interface ShadowListCommands {
  setStartReachedEnabled: (enabled: boolean) => void;
  setEndReachedEnabled: (enabled: boolean) => void;
  /*
   * Bring the row at `index` into view. `viewPosition` places it within the viewport:
   * 0 (the default) aligns it to the start, 0.5 centres it, 1 aligns it to the end.
   */
  scrollToIndex: (index: number, viewPosition?: number) => void;
  scrollToOffset: (offset: number, animated?: boolean) => void;
  scrollToEnd: (animated?: boolean) => void;
  /*
   * Laid-out size of a mounted row along the scroll axis, or undefined when the row is
   * not mounted, not yet laid out, or `trackElementSizes` is off.
   */
  getElementSize: (key: string) => number | undefined;
  // Every size recorded so far. Live: the map is the store, not a copy of it.
  getElementSizes: () => ReadonlyMap<string, number>;
}

export interface ViewabilityConfig {
  itemVisiblePercentThreshold?: number;
}

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
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
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
  dragEnabled?: boolean;
  onReorder?: (info: { from: number; to: number; data: ElementT[] }) => void;
  stickyHeaderIndices?: ReadonlyArray<number>;
  renderStickyHeaderOverlay?: (activeIndex: number) => ReactElement | null;
  columns?: number;
  /*
   * How far beyond the visible window the core measures and lays out elements, in
   * viewports. Native geometry work, not React. See overscanRows for the React side.
   */
  overscan?: number;
  /*
   * How many rows React keeps mounted on each side of the visible window. The right
   * number depends on row height: short rows (a chat, a contact list) can afford the
   * default 4 behind and 10 ahead, while a feed of full-screen cards wants 1-2, since ten
   * rows ahead is ten viewports of mounted React paid for on every fling. Set too low, a
   * fling shows blank cells -- raise it until they stop, not past.
   */
  overscanRows?: number;
  /*
   * Rows mounted ahead of the visible window in the direction of travel, replacing
   * overscanRows on that side while a fling is in progress. A resting list uses
   * overscanRows on both sides.
   */
  overscanRowsLeading?: number;
  getElementSizeSpec?: (
    element: ElementT,
    index: number
  ) => ElementSizeSpec | null | undefined;
  measureLookaheadRows?: number;
  persistentKeys?: ReadonlyArray<string>;
  nonAnchorKeys?: ReadonlyArray<string>;
  containerOffsetIndex?: number;
  /*
   * Record each mounted row's laid-out size along the scroll axis, readable through
   * `getElementSize` / `getElementSizes` on the ref. Off by default: it puts an onLayout
   * on every mounted row. Turn it on to position something against a row, such as a
   * context menu or a highlight overlay.
   *
   * Set it for the life of the list: rows already mounted when it is turned on report
   * nothing until they are next laid out, and turning it off drops every size recorded.
   */
  trackElementSizes?: boolean;
  refreshing?: boolean;
  onRefresh?: () => void;
  refreshColor?: ColorValue;
  initialElementsSize?: number;
  onStartReached?: () => void;
  onEndReached?: () => void;
  onStartReachedThreshold?: number;
  onEndReachedThreshold?: number;
  onScroll?: (event: { nativeEvent: OnScroll }) => void;
  snapToItem?: boolean;
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
  renderElement?: SectionListRenderElement<ElementT, SectionT>;
  keyExtractor?: (element: ElementT, index: number) => string;
}

export type SectionListData<ElementT, SectionT = object> = SectionT &
  SectionBase<ElementT, SectionT>;

export interface SectionListRenderElementInfo<ElementT, SectionT = object> {
  element: ElementT;
  index: number;
  section: SectionListData<ElementT, SectionT>;
}

export type SectionListRenderElement<ElementT, SectionT = object> = (
  info: SectionListRenderElementInfo<ElementT, SectionT>
) => ReactElement | null;

/*
 * The ShadowList props SectionList and TreeList forward unchanged. Everything that addresses
 * rows (data, renderElement, keyExtractor, size specs, viewability tokens, reordering, sticky
 * indices) is left out: those lists render flattened rows the caller never sees, so they
 * define their own versions or do not offer the prop.
 */
export type ShadowListForwardedProps = Omit<
  ShadowListProps<{ id: string }>,
  | 'data'
  | 'renderElement'
  | 'keyExtractor'
  | 'getElementSizeSpec'
  | 'stickyHeaderIndices'
  | 'renderStickyHeaderOverlay'
  | 'dragEnabled'
  | 'onReorder'
  | 'viewabilityConfig'
  | 'onViewableItemsChanged'
>;

export interface SectionListProps<ElementT, SectionT = object> extends Omit<
  ShadowListForwardedProps,
  'ItemSeparatorComponent' | 'nonAnchorKeys' | 'persistentKeys'
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
  ItemSeparatorComponent?: ReactElement | (() => ReactElement | null) | null;
  SectionSeparatorComponent?: ReactElement | (() => ReactElement | null) | null;
  getElementSizeSpec?: (
    element: ElementT,
    index: number,
    section: SectionListData<ElementT, SectionT>
  ) => ElementSizeSpec | null | undefined;
  nonAnchorKeys?: ReadonlyArray<string>;
  persistentKeys?: ReadonlyArray<string>;
}

/*
 * TreeList types. The visible subtree (nodes whose ancestors are all expanded) is
 * flattened into one element stream; collapsed subtrees are never walked.
 * `keyExtractor` must return a globally unique, stable id per node (stable across
 * expand/collapse) so reconcile and measurement caching line up.
 */
export interface TreeListRenderElementInfo<ElementT> {
  element: ElementT;
  index: number;
  depth: number;
  isExpanded: boolean;
  hasChildren: boolean;
  indent: number;
  toggle: () => void;
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
}

/*
 * ShadowListNative types. Rows are clones of a template, made natively; data lives in a native
 * store and templates read it through `bind`. See SHADOWLIST_NATIVE.md.
 */

/*
 * Prop name -> expression over the row's item:
 *   'author.name'        the value at that path ('images.0.uri' indexes arrays)
 *   '!isRead'            the negated truthiness of the value
 *   '{name} · {date}'    a format string
 * Special props: `text` (a Text's content), `uri` (an Image's source), `hidden` / `visible`
 * (display). Color props accept CSS color strings. Anything else is passed through as the raw
 * prop value.
 */
export type ShadowListNativeBind = Record<string, string>;

export interface ShadowListNativeElementProps {
  // Names the element for setTemplateStyle.
  id?: string;
  bind?: ShadowListNativeBind;
  // Makes the element pressable; presses arrive as onElementPress with this action.
  action?: string;
}

export interface ShadowListNativeViewProps extends ShadowListNativeElementProps {
  /*
   * Path of an array in the item. The view's children are one entry's template, cloned once
   * per entry (up to `repeatMax`); `bind` paths inside resolve against the entry ('.' is the
   * entry itself). The view's own `bind` still reads the row.
   */
  repeat?: string;
  repeatMax?: number;
}

export interface ShadowListNativeElementPressEvent<ItemT> {
  key: string;
  index: number;
  action: string;
  elementId?: string;
  item: ItemT | undefined;
  // Entry of the innermost `repeat` the pressed element is in; undefined outside one.
  repeatIndex?: number;
}

export interface ShadowListNativeSetDataOptions {
  /*
   * 'start': offset 0 (header in view) in the same commit as the new rows. setData followed by
   * scrollToStart() is two corrections: MVCP holds the old first row for the frames in between.
   */
  scrollTo?: 'start';
}

export interface ShadowListNativeCommands<ItemT> {
  // Shallow-merges `patch` into the row's item. Only that row is rebuilt. False if unknown.
  updateItem: (key: string, patch: Partial<ItemT>) => boolean;
  replaceItem: (key: string, item: ItemT) => boolean;
  insertItems: (index: number, items: ReadonlyArray<ItemT>) => number;
  appendItems: (items: ReadonlyArray<ItemT>) => number;
  prependItems: (items: ReadonlyArray<ItemT>) => number;
  removeItems: (keys: ReadonlyArray<string>) => number;
  moveItem: (key: string, toIndex: number) => boolean;
  setData: (
    items: ReadonlyArray<ItemT>,
    options?: ShadowListNativeSetDataOptions
  ) => number;
  // Style merged over one template element (by its `id`) in every row; null clears it.
  setTemplateStyle: (
    template: string,
    elementId: string,
    style: ViewStyle | TextStyle | null
  ) => void;
  getItem: (key: string) => ItemT | undefined;
  getKeys: () => string[];
  getCount: () => number;
  setStartReachedEnabled: (enabled: boolean) => void;
  setEndReachedEnabled: (enabled: boolean) => void;
  scrollToIndex: (index: number, viewPosition?: number) => void;
  scrollToOffset: (offset: number, animated?: boolean) => void;
  scrollToEnd: (animated?: boolean) => void;
  // Offset 0 (header in view), after every mutation made before it is laid out.
  scrollToStart: () => void;
}

/*
 * Where the rows come from; exactly one of the two.
 *
 * - `initialData` (uncontrolled): seeds the native store once, when the list mounts. Later
 *   arrays are ignored; the store is changed only through the ref's commands (appendItems,
 *   updateItem, setData, ...). Use it when the list owns its rows: paging, local edits.
 * - `data` (controlled): the store mirrors the array. Each new array replaces the store (keyed:
 *   rows whose item is unchanged keep their views), so a command's change lasts only until the
 *   next array; write edits into the source of `data` instead.
 */
export type ShadowListNativeDataProps<ItemT> =
  | { data: ReadonlyArray<ItemT>; initialData?: never }
  | { initialData: ReadonlyArray<ItemT>; data?: never };

export type ShadowListNativeProps<ItemT> = ShadowListNativeDataProps<ItemT> &
  ShadowListNativeListProps<ItemT>;

export interface ShadowListNativeListProps<ItemT> {
  keyExtractor?: (item: ItemT, index: number) => string;
  // Template name -> element. Declared once; every row is a clone of one of them.
  templates: Readonly<Record<string, ReactElement>>;
  // Item field naming a row's template, or a function returning it. Default: 'default' or the first.
  templateKey?: string;
  getTemplate?: (item: ItemT, index: number) => string;
  onElementPress?: (event: ShadowListNativeElementPressEvent<ItemT>) => void;
  onVisibleRangeChange?: (range: { start: number; end: number }) => void;
  style?: ViewStyle;
  elementStyle?: ViewStyle;
  testID?: string;
  inverted?: boolean;
  // Scroll onto rows appended below the newest one while the list is at its end (see ShadowList).
  followAppends?: boolean;
  horizontal?: boolean;
  columns?: number;
  // Viewports measured and mounted beyond the visible one, on each side.
  overscan?: number;
  // Rows mounted before the list knows its viewport.
  initialNumToRender?: number;
  // Extra rows mounted past the window whenever it moves, so small scrolls rebuild nothing.
  padRows?: number;
  // Rows kept (unmounted) for reuse when they scroll back in.
  cacheRows?: number;
  initialScrollIndex?: number;
  stickyHeader?: boolean;
  stickyFooter?: boolean;
  autoHideHeader?: boolean;
  autoHideFooter?: boolean;
  snapToItem?: boolean;
  snapToAlignment?: 'start' | 'center' | 'end';
  refreshing?: boolean;
  onRefresh?: () => void;
  // Once per refresh, after `refreshing` turns false and the spinner is gone: apply new rows here.
  onRefreshSettle?: () => void;
  refreshColor?: ColorValue;
  onStartReached?: () => void;
  onEndReached?: () => void;
  onStartReachedThreshold?: number;
  onEndReachedThreshold?: number;
  onScroll?: (event: { nativeEvent: OnScroll }) => void;
  ListHeaderComponent?: ReactElement | (() => ReactElement | null) | null;
  ListFooterComponent?: ReactElement | (() => ReactElement | null) | null;
  ListEmptyComponent?: ReactElement | (() => ReactElement | null) | null;
}
