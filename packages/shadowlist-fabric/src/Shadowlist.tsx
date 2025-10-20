import { useState, useMemo, useCallback, type ReactElement } from 'react';
import { StyleSheet, type CodegenTypes, type ViewStyle } from 'react-native';
import {
  ShadowlistView,
  ShadowlistElementView,
  type OnVisibleIndicesChange,
} from 'react-native-shadowlist';

const INVERTED = true;

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

export const inversionBasedInitialIndices = (inverted: boolean) => {
  if (inverted) {
    return {
      visibleStartIndex: 980,
      visibleEndIndex: 999,
    };
  } else {
    return {
      visibleStartIndex: 0,
      visibleEndIndex: 20,
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

export interface ShadowListProps {
  renderItem: ({ index }: { index: number }) => ReactElement;
  style?: ViewStyle;
  itemStyle?: ViewStyle;
}

function ShadowList({ renderItem, style, itemStyle }: ShadowListProps) {
  const [visibleIndices, setVisibleIndices] = useState<OnVisibleIndicesChange>(
    inversionBasedInitialIndices(INVERTED)
  );

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

      return inversionBasedUpdatingIndices(nextIndices, INVERTED);
    });
  }, []);

  const visibleRange = useMemo(
    () => createRangeArray(visibleIndices),
    [visibleIndices]
  );

  return (
    <ShadowlistView
      style={[styles.container, style]}
      onVisibleIndicesChange={handleVisibleIndicesChange}
    >
      {visibleRange.map((index) => {
        return (
          <ShadowlistElementView
            index={index}
            style={[styles.item, itemStyle]}
            key={index}
          >
            {renderItem({ index })}
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
