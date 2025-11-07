import type { ComponentRef, Ref } from 'react';
import { useRef } from 'react';
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
  type OnVisibleIndicesChange,
  Commands,
} from 'shadowlist';

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

export interface ShadowListCommands {}

export interface ShadowListProps<ElementT extends { id: string } = any> {
  data: ReadonlyArray<ElementT>;
  renderElement: (info: { element: ElementT; index: number }) => ReactElement;
  style?: ViewStyle;
  elementStyle?: ViewStyle;
  inverted?: boolean;
  horizontal?: boolean;
  ref?: Ref<ShadowListCommands>;
  onStartReached?: () => void;
  onEndReached?: () => void;
}

function ShadowList<ElementT extends { id: string } = any>({
  data,
  renderElement,
  style,
  elementStyle,
  inverted = false,
  horizontal = false,
  ref,
  onStartReached,
  onEndReached,
}: ShadowListProps<ElementT>) {
  const shadowlistViewRef = useRef<ComponentRef<typeof ShadowlistView> | null>(
    null
  );

  const [visibleIndices, setVisibleIndices] = useState<OnVisibleIndicesChange>(
    inversionBasedInitialIndices(data.length, 20, inverted)
  );

  const elementBaseStyle = useMemo(
    () => [
      styles.element,
      horizontal ? styles.elementHorizontal : styles.elementVertical,
      elementStyle,
    ],
    [horizontal, elementStyle]
  );

  useImperativeHandle(ref, () => ({
    setStartReachedEnabled: (enabled: boolean) => {
      if (!shadowlistViewRef.current) return;

      Commands.setStartReachedEnabled(shadowlistViewRef.current, enabled);
    },
    setEndReachedEnabled: (enabled: boolean) => {
      if (!shadowlistViewRef.current) return;

      Commands.setEndReachedEnabled(shadowlistViewRef.current, enabled);
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

        return inversionBasedUpdatingIndices(nextIndices, inverted);
      });
    },
    [inverted]
  );

  const visibleRange = useMemo(
    () => createRangeArray(visibleIndices),
    [visibleIndices]
  );

  const elementsAllKeys = useMemo(() => data.map((element) => element.id), [data]);
  const elementsHeadKey = useMemo(() => data.at(0)?.id, [data]);
  const elementsTailKey = useMemo(() => data.at(-1)?.id, [data]);

  return (
    <ShadowlistView
      ref={shadowlistViewRef}
      style={[styles.container, style]}
      onVisibleIndicesChange={handleVisibleIndicesChange}
      elementsAllKeys={elementsAllKeys}
      elementsHeadKey={elementsHeadKey}
      elementsTailKey={elementsTailKey}
      inverted={inverted}
      horizontal={horizontal}
      onStartReached={onStartReached}
      onEndReached={onEndReached}
    >
      {visibleRange.map((index) => {
        const element = data[index];

        if (!element) return null;

        return (
          <ShadowlistElementView
            index={index}
            style={elementBaseStyle}
            key={element.id}
          >
            {renderElement({ element, index })}
          </ShadowlistElementView>
        );
      })}
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

export default ShadowList;
