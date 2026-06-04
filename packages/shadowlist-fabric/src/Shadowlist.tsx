import type { ComponentRef, Ref } from 'react';
import { useRef, memo } from 'react';
import {
  useState,
  useMemo,
  useImperativeHandle,
  useCallback,
  type ReactElement,
} from 'react';
import { StyleSheet, type CodegenTypes, type ViewStyle } from 'react-native';
import {
  ShadowlistView,
  ShadowlistElementView,
  ShadowlistTemplateView,
  type OnVisibleIndicesChange,
  type OnScroll,
  Commands,
} from 'shadowlist';

/*
 * Trace the JS <-> native state synchronization. Mirrors the SHADOWLIST_DEBUG_LOG
 * flag in the C++ core (shadowlist-core/Constants.hpp) and shares the same [SL]
 * tag, so the JS, C++ and iOS/Android logs interleave into one readable stream.
 * Only the meaningful boundaries are logged (native events applied upward,
 * imperative commands sent downward) to keep it off the per-frame scroll path.
 */
const SHADOWLIST_DEBUG_LOG = true;

const slLog = (...args: unknown[]) => {
  if (!SHADOWLIST_DEBUG_LOG) return;
  console.log('[SL]', ...args);
};

function createRangeArray(indices: OnVisibleIndicesChange) {
  if (
    indices.visibleStartIndex === -1 ||
    indices.visibleEndIndex === -1 ||
    indices.visibleStartIndex > indices.visibleEndIndex
  ) {
    return [];
  }
  const length = indices.visibleEndIndex - indices.visibleStartIndex + 1;
  return Array.from(
    { length },
    (_, index) => indices.visibleStartIndex + index
  );
}

export const inversionBasedInitialIndices = (
  size: number,
  initial: number,
  inverted: boolean
) => {
  if (inverted) {
    return {
      visibleStartIndex: size - initial,
      visibleEndIndex: size - 1,
    };
  } else {
    return {
      visibleStartIndex: 0,
      visibleEndIndex: initial,
    };
  }
};

export const inversionBasedUpdatingIndices = (
  indices: OnVisibleIndicesChange,
  inverted: boolean
) => {
  if (inverted) {
    return {
      visibleStartIndex: indices.visibleEndIndex,
      visibleEndIndex: indices.visibleStartIndex,
    };
  } else {
    return {
      visibleStartIndex: indices.visibleStartIndex,
      visibleEndIndex: indices.visibleEndIndex,
    };
  }
};

interface ElementRendererProps<ElementT> {
  element: ElementT;
  index: number;
  style: ViewStyle | ViewStyle[];
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
}

const ElementRenderer = memo(function ElementRenderer<
  ElementT extends { id: string },
>({ element, index, style, renderElement }: ElementRendererProps<ElementT>) {
  return (
    <ShadowlistElementView index={index} style={style} key={element.id}>
      {renderElement({ element, index })}
    </ShadowlistElementView>
  );
}) as <T extends { id: string }>(
  props: ElementRendererProps<T>
) => ReactElement;

export interface ShadowlistCommands {
  setStartReachedEnabled: (enabled: boolean) => void;
  setEndReachedEnabled: (enabled: boolean) => void;
  scrollToIndex: (index: number) => void;
}

export interface ShadowlistProps<ElementT extends { id: string }> {
  data: ReadonlyArray<ElementT>;
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
  style?: ViewStyle;
  elementStyle?: ViewStyle;
  inverted?: boolean;
  horizontal?: boolean;
  columns?: number;
  containerOffsetIndex?: number;
  initialElementsSize?: number;
  ref?: Ref<ShadowlistCommands>;
  onStartReached?: () => void;
  onEndReached?: () => void;
  onScroll?: (event: { nativeEvent: OnScroll }) => void;
  ListHeaderComponent?: ReactElement | (() => ReactElement | null) | null;
  ListFooterComponent?: ReactElement | (() => ReactElement | null) | null;
  ListEmptyComponent?: ReactElement | (() => ReactElement | null) | null;
}

function Shadowlist<ElementT extends { id: string }>({
  data,
  renderElement,
  style,
  elementStyle,
  inverted = false,
  horizontal = false,
  columns = 1,
  containerOffsetIndex = -2,
  initialElementsSize = 20,
  ref,
  onStartReached,
  onEndReached,
  onScroll,
  ListHeaderComponent,
  ListFooterComponent,
  ListEmptyComponent,
}: ShadowlistProps<ElementT>) {
  const shadowlistViewRef = useRef<ComponentRef<typeof ShadowlistView> | null>(
    null
  );

  const [visibleIndices, setVisibleIndices] = useState<OnVisibleIndicesChange>(
    inversionBasedInitialIndices(data.length, initialElementsSize, inverted)
  );

  const elementDimensionStyle = useMemo(() => {
    if (horizontal) {
      return columns > 1
        ? { height: `${100 / columns}%` }
        : styles.elementHorizontal;
    } else {
      return columns > 1
        ? { width: `${100 / columns}%` }
        : styles.elementVertical;
    }
  }, [horizontal, columns]);

  const elementBaseStyle = useMemo(
    () =>
      elementStyle
        ? [styles.element, elementDimensionStyle, elementStyle]
        : [styles.element, elementDimensionStyle],
    [elementDimensionStyle, elementStyle]
  );

  useImperativeHandle(ref, () => ({
    setStartReachedEnabled: (enabled: boolean) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd setStartReachedEnabled', `enabled=${enabled ? 1 : 0}`);
      Commands.setStartReachedEnabled(shadowlistViewRef.current, enabled);
    },
    setEndReachedEnabled: (enabled: boolean) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd setEndReachedEnabled', `enabled=${enabled ? 1 : 0}`);
      Commands.setEndReachedEnabled(shadowlistViewRef.current, enabled);
    },
    scrollToIndex: (index: number) => {
      if (!shadowlistViewRef.current) return;

      slLog('js.cmd scrollToIndex', `index=${index}`);
      Commands.scrollToIndex(shadowlistViewRef.current, index);
    },
  }));

  const handleVisibleIndicesChange: CodegenTypes.DirectEventHandler<
    OnVisibleIndicesChange,
    never
  > = useCallback(
    (event) => {
      const nextIndices = event.nativeEvent;
      setVisibleIndices((prevIndices) => {
        const startDiff = Math.abs(
          nextIndices.visibleStartIndex - prevIndices.visibleStartIndex
        );
        const endDiff = Math.abs(
          nextIndices.visibleEndIndex - prevIndices.visibleEndIndex
        );

        // If change is within +-1 steps, don't update
        if (startDiff <= 1 && endDiff <= 1) {
          return prevIndices;
        }

        const updatedIndices = inversionBasedUpdatingIndices(
          nextIndices,
          inverted
        );
        slLog(
          'js.onVisibleIndicesChange apply',
          `native=[${nextIndices.visibleStartIndex}..${nextIndices.visibleEndIndex}]`,
          `render=[${updatedIndices.visibleStartIndex}..${updatedIndices.visibleEndIndex}]`,
          `inv=${inverted ? 1 : 0}`
        );
        return updatedIndices;
      });
    },
    [inverted]
  );

  const visibleRange = useMemo(
    () => createRangeArray(visibleIndices),
    [visibleIndices]
  );

  const elementsAllKeys = useMemo(
    () => data.map((element) => element.id),
    [data]
  );

  const renderComponent = (
    component: ReactElement | (() => ReactElement | null) | null | undefined
  ): ReactElement | null => {
    if (!component) return null;
    return typeof component === 'function' ? component() : component;
  };

  const header = useMemo(
    () => renderComponent(ListHeaderComponent),
    [ListHeaderComponent]
  );

  const footer = useMemo(
    () => renderComponent(ListFooterComponent),
    [ListFooterComponent]
  );

  const empty = useMemo(
    () => renderComponent(ListEmptyComponent),
    [ListEmptyComponent]
  );

  return (
    <ShadowlistView
      ref={shadowlistViewRef}
      style={[styles.container, style]}
      onVisibleIndicesChange={handleVisibleIndicesChange}
      elementsAllKeys={elementsAllKeys}
      inverted={inverted}
      horizontal={horizontal}
      columns={columns}
      containerOffsetIndex={containerOffsetIndex}
      onStartReached={onStartReached}
      onEndReached={onEndReached}
      onScroll={onScroll}
    >
      {header && (
        <ShadowlistTemplateView templateType="header">
          {header}
        </ShadowlistTemplateView>
      )}
      {data.length === 0 && empty ? (
        <ShadowlistTemplateView templateType="empty">
          {empty}
        </ShadowlistTemplateView>
      ) : (
        visibleRange.map((index) => {
          const element = data[index];

          if (!element) return null;

          return (
            <ElementRenderer
              key={element.id}
              element={element}
              index={index}
              style={elementBaseStyle}
              renderElement={renderElement}
            />
          );
        })
      )}
      {footer && (
        <ShadowlistTemplateView templateType="footer">
          {footer}
        </ShadowlistTemplateView>
      )}
    </ShadowlistView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  element: {
    position: 'absolute',
  },
  elementVertical: {
    width: '100%',
  },
  elementHorizontal: {
    height: '100%',
  },
});

export default Shadowlist;
