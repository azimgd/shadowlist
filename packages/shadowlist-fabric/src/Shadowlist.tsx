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

export interface ShadowListProps<ItemT extends { id: string } = any> {
  data: ReadonlyArray<ItemT>;
  renderItem: (info: { item: ItemT; index: number }) => ReactElement;
  style?: ViewStyle;
  itemStyle?: ViewStyle;
  inverted?: boolean;
  ref?: Ref<ShadowListCommands>;
  onStartReached?: () => void;
  onEndReached?: () => void;
}

function ShadowList<ItemT extends { id: string } = any>({
  data,
  renderItem,
  style,
  itemStyle,
  inverted = false,
  ref,
  onStartReached,
  onEndReached,
}: ShadowListProps<ItemT>) {
  const shadowlistViewRef = useRef<ComponentRef<typeof ShadowlistView> | null>(
    null
  );

  const [visibleIndices, setVisibleIndices] = useState<OnVisibleIndicesChange>(
    inversionBasedInitialIndices(data.length, 20, inverted)
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
      onStartReached={onStartReached}
      onEndReached={onEndReached}
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
    opacity: 0,
  },
});

export default ShadowList;
