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
  scrollToIndex: (index: number) => void;
  scrollToOffset: (offset: number, animated?: boolean) => void;
  scrollToEnd: (animated?: boolean) => void;
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
  overscan?: number;
  getElementSizeSpec?: (
    element: ElementT,
    index: number
  ) => ElementSizeSpec | null | undefined;
  measureLookaheadRows?: number;
  persistentKeys?: ReadonlyArray<string>;
  nonAnchorKeys?: ReadonlyArray<string>;
  containerOffsetIndex?: number;
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
  scrollToNode: (id: string) => void;
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
