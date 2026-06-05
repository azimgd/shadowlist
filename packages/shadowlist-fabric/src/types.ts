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
