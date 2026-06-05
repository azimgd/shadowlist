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
