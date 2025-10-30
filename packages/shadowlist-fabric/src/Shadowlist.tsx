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

export interface ShadowListCommands {
  prependElements: (size: number) => void;
  appendElements: (size: number) => void;
}

export interface ShadowListProps<ItemT extends { id: string } = any> {
  data: ReadonlyArray<ItemT>;
  renderItem: (info: { item: ItemT; index: number }) => ReactElement;
  style?: ViewStyle;
  itemStyle?: ViewStyle;
  inverted?: boolean;
  ref?: Ref<ShadowListCommands>;
}

function ShadowList<ItemT extends { id: string } = any>({
  data,
  renderItem,
  style,
  itemStyle,
  inverted = false,
  ref,
}: ShadowListProps<ItemT>) {
  const shadowlistViewRef = useRef<ComponentRef<typeof ShadowlistView> | null>(
    null
  );

  const [visibleIndices, setVisibleIndices] = useState<OnVisibleIndicesChange>(
    inversionBasedInitialIndices(data.length, 20, inverted)
  );

  useImperativeHandle(ref, () => ({
    prependElements: (size: number) => {
      if (!shadowlistViewRef.current) return;

      Commands.prependElements(shadowlistViewRef.current, size);

      setVisibleIndices((prev) => ({
        visibleStartIndex: prev.visibleStartIndex + size,
        visibleEndIndex: prev.visibleEndIndex + size,
      }));
    },
    appendElements: (size: number) => {
      if (!shadowlistViewRef.current) return;

      Commands.appendElements(shadowlistViewRef.current, size);
    },
  }));

  const handleVisibleIndicesChange: CodegenTypes.DirectEventHandler<
    OnVisibleIndicesChange,
    never
  > = useCallback((event) => {
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
  }, [inverted]);

  const visibleRange = useMemo(
    () => createRangeArray(visibleIndices),
    [visibleIndices]
  );

  const elementsAllKeys = useMemo(() => data.map((item) => item.id), [data]);
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
      horizontal={false}
    >
      {visibleRange.map((index) => {
        const item = data[index];

        if (!item) return null;

        return (
          <ShadowlistElementView
            index={index}
            style={[styles.item, itemStyle]}
            key={item.id}
          >
            {renderItem({ item, index })}
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
  item: {
    width: '100%',
    position: 'absolute',
  },
});

export default ShadowList;
