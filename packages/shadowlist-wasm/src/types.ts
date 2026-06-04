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
}

type ListComponent = ReactElement | (() => ReactElement | null) | null;

/*
 * Public props. Kept aligned with the native package so application code is
 * portable across platforms.
 */
export interface ShadowlistProps<ElementT extends { id: string }> {
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
  onScroll?: (event: { nativeEvent: OnScroll }) => void;

  ListHeaderComponent?: ListComponent;
  ListFooterComponent?: ListComponent;
  ListEmptyComponent?: ListComponent;
}
