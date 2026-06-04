import type { CSSProperties, ReactElement, ReactNode } from 'react';

/*
 * Scroll offset payload, matching the React Native onScroll event shape.
 */
export interface OnScrollEvent {
  contentOffsetX: number;
  contentOffsetY: number;
}

/*
 * Imperative handle exposed via ref, mirroring the React Native ShadowList
 * commands (setStartReachedEnabled / setEndReachedEnabled / scrollToIndex).
 */
export interface ShadowListCommands {
  setStartReachedEnabled: (enabled: boolean) => void;
  setEndReachedEnabled: (enabled: boolean) => void;
  scrollToIndex: (index: number) => void;
}

type ListComponent = ReactElement | (() => ReactElement | null) | null;

/*
 * Public props, intentionally aligned with the React Native Shadowlist props so
 * application code is portable between the native and web integrations.
 */
export interface ShadowListProps<ElementT extends { id: string }> {
  data: ReadonlyArray<ElementT>;
  renderElement: (info: { element: ElementT; index: number }) => ReactNode;

  style?: CSSProperties;
  elementStyle?: CSSProperties;

  inverted?: boolean;
  horizontal?: boolean;
  columns?: number;

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
  onScroll?: (event: { nativeEvent: OnScrollEvent }) => void;

  ListHeaderComponent?: ListComponent;
  ListFooterComponent?: ListComponent;
  ListEmptyComponent?: ListComponent;
}
