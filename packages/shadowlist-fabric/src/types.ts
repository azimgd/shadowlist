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
