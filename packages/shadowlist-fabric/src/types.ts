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
   * Scroll the row at index into view. viewPosition 0 puts it at the start, which is
   * the default, 0.5 in the middle and 1 at the end.
   */
  scrollToIndex: (index: number, viewPosition?: number) => void;
  scrollToOffset: (offset: number, animated?: boolean) => void;
  scrollToEnd: (animated?: boolean) => void;
  /*
   * Size of a mounted row along the scroll axis. Undefined if the row is not mounted,
   * not laid out yet, or trackElementSizes is off.
   */
  getElementSize: (key: string) => number | undefined;
  // Every size recorded so far. This is the live map, not a copy.
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
   * How many viewports past the visible area the core measures and lays out rows.
   * This is native work only. overscanRows controls how much React mounts.
   */
  overscan?: number;
  /*
   * How many rows React keeps mounted on each side of the visible area. Short rows like
   * chat or contacts can afford the default of 4 behind and 10 ahead. A feed of full
   * screen cards wants 1 or 2, since 10 rows ahead means 10 screens of React per fling.
   * If a fling shows blank cells, raise it until they stop, but no further.
   */
  overscanRows?: number;
  /*
   * Rows mounted ahead in the scroll direction during a fling, in place of overscanRows
   * on that side. A list at rest uses overscanRows on both sides.
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
   * Record each mounted row's size along the scroll axis, readable with getElementSize
   * and getElementSizes on the ref. Off by default since it adds an onLayout to every
   * mounted row. Turn it on to place something next to a row, like a context menu.
   *
   * Set it once for the life of the list. Rows already mounted report nothing until
   * their next layout, and turning it off drops every recorded size.
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
 * TreeList types. Nodes whose parents are all expanded are flattened into one list, and
 * collapsed branches are never walked. keyExtractor must return an id that is unique
 * across the whole tree and stays the same when nodes expand or collapse, so rows and
 * cached sizes stay matched.
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
 * ShadowListNative types. Rows are native copies of a template. The data lives in a native
 * store and templates read it with bind. See SHADOWLIST_NATIVE.md.
 */

/*
 * Maps a prop name to an expression over the row's item.
 *   'author.name'        the value at that path, and 'images.0.uri' indexes arrays
 *   '!isRead'            true when the value is falsy
 *   '{name} · {date}'    a format string
 * A few props are special. text sets a Text's content, uri sets an Image's source, and
 * hidden or visible toggle display. Color props take CSS color strings. Any other prop gets
 * the raw value.
 */
export type ShadowListNativeBind = Record<string, string>;

export interface ShadowListNativeElementProps {
  // Names the element for setTemplateStyle.
  id?: string;
  bind?: ShadowListNativeBind;
  // Makes the element pressable. Presses arrive as onElementPress with this action.
  action?: string;
}

export interface ShadowListNativeViewProps extends ShadowListNativeElementProps {
  /*
   * Path of an array in the item. The view's children are the template for one entry and
   * get copied once per entry, up to repeatMax. Bind paths inside read from the entry, and
   * '.' is the entry itself. The view's own bind still reads the row.
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
  // Index in the innermost repeat around the pressed element, or undefined if there is none.
  repeatIndex?: number;
  // Where the touch ended for a press, or rested for a long press, in window coordinates.
  pageX: number;
  pageY: number;
}

export interface ShadowListNativeSetDataOptions {
  /*
   * 'start' scrolls to the top, header in view, in the same commit as the new rows. Calling
   * scrollToStart after setData takes two steps, and the list keeps the old first row in place
   * for the frames in between.
   */
  scrollTo?: 'start';
}

export interface ShadowListNativeCommands<ItemT> {
  // Merges patch into the row's item and rebuilds only that row. False if the key is unknown.
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
  // Merges a style over one template element, found by id, in every row. Null clears it.
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
  // Scrolls to the top with the header in view, after every change made before it is laid out.
  scrollToStart: () => void;
  // Indexed lists only. Runs getExtra again for the built rows, or just the given indices.
  refreshExtras: (indices?: Iterable<number>) => void;
}

/*
 * Where the rows come from. Pass exactly one.
 *
 * initialData fills the native store once, on mount. Later arrays are ignored and only the
 * ref's commands like appendItems, updateItem or setData change the store. Use it when the
 * list owns its rows, for paging or local edits.
 *
 * data keeps the store in sync with the array. Each new array replaces the store by key, and
 * rows whose item didn't change keep their views. A command's change lasts only until the
 * next array, so make edits in the source of data instead.
 */
export type ShadowListNativeDataProps<ItemT> =
  | { data: ReadonlyArray<ItemT>; initialData?: never; indexed?: never }
  | { initialData: ReadonlyArray<ItemT>; data?: never; indexed?: never }
  | {
      indexed: ShadowListNativeIndexedData<ItemT>;
      data?: never;
      initialData?: never;
    };

/*
 * Rows by position, for very long lists where each row is mostly a number. There are count
 * rows keyed "0", "1" and so on, and none of them are stored. Row i's item is built natively
 * as { [indexField]: i, [valueField]: order[i] }, where order[i] is i if there is no order.
 * Its entry in extras is merged in, but those two fields win.
 * A new object replaces the rows, like data does. With the same count the keys stay the
 * same, so only mounted rows whose item changed get rebound. updateItem(String(i), patch)
 * merges into row i's extra. insertItems, removeItems and moveItem do nothing here.
 */
export interface ShadowListNativeIndexedData<ItemT> {
  count: number;
  // One 32 bit value per row, copied to native in one go.
  order?: Int32Array;
  /*
   * One stable id per row, so row i's key is String(ids[i]) instead of its position. An
   * insert, remove or move is then just a new order and ids, matched by key while the visible
   * rows stay in place. Without ids the keys are positions, which is cheapest and fits when
   * rows are replaced in place, like after a sort or refresh.
   */
  ids?: Int32Array;
  // Field names for the position and the order value. Defaults are 'index' and 'value'.
  indexField?: string;
  valueField?: string;
  // Rows that need more than those two fields. Their template comes from templateKey or getTemplate.
  extras?: ReadonlyArray<{ index: number; item: Partial<ItemT> }>;
}

export type ShadowListNativeProps<ItemT> = ShadowListNativeDataProps<ItemT> &
  ShadowListNativeListProps<ItemT>;

export interface ShadowListNativeListProps<ItemT> {
  keyExtractor?: (item: ItemT, index: number) => string;
  // Templates by name. Every row is a copy of one of them.
  templates: Readonly<Record<string, ReactElement>>;
  // Item field that names the row's template, or a function that returns it. Falls back to 'default' or the first.
  templateKey?: string;
  getTemplate?: (item: ItemT, index: number) => string;
  onElementPress?: (event: ShadowListNativeElementPressEvent<ItemT>) => void;
  /*
   * Fires when a touch rests on an element with an action for longPressDelay ms. When set, a
   * long press does not also fire onElementPress on release.
   */
  onElementLongPress?: (
    event: ShadowListNativeElementPressEvent<ItemT>
  ) => void;
  // Default 500.
  longPressDelay?: number;
  /*
   * Indexed lists only. Returns fields for a row, merged over its static extra. It runs for
   * rows near the visible area, extraPadding screens each side and at least 20 rows. Rows
   * further out only get these fields as they come closer, so after a long fling a row can
   * show them a frame late. A new function reruns for every built row, so memoize it. Call
   * refreshExtras when state it reads changes without a new function.
   */
  getExtra?: (index: number) => Partial<ItemT> | null | undefined;
  // Default 1.
  extraPadding?: number;
  /*
   * Rows that stick to the top while their section scrolls by, like section headers, until
   * the next one pushes them off. The pinned row is a native copy made in the same commit, and
   * presses on it go to the real row. Doesn't work with inverted.
   */
  stickyHeaderIndices?: ReadonlyArray<number>;
  onVisibleRangeChange?: (range: { start: number; end: number }) => void;
  style?: ViewStyle;
  elementStyle?: ViewStyle;
  testID?: string;
  inverted?: boolean;
  // Follow rows appended at the end while the list is scrolled to its end, like ShadowList.
  followAppends?: boolean;
  horizontal?: boolean;
  columns?: number;
  // Screens measured and mounted past the visible one, on each side.
  overscan?: number;
  // Rows mounted before the list knows its viewport.
  initialNumToRender?: number;
  // Extra rows mounted past the visible area, so small scrolls rebuild nothing.
  padRows?: number;
  // Unmounted rows kept for reuse when they scroll back in.
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
  // Runs once per refresh after refreshing turns false and the spinner is gone. Apply new rows here.
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
