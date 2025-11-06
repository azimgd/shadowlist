import { useState, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { Shadowlist, type ShadowListCommands } from 'shadowlist';
import { FloatingActionBar } from './FloatingActionBar';
import { GridElement, type GridElement as GridElementType } from './GridElement';
import { generateGridElement } from './constants';

export const GridScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const [data, setData] = useState<GridElementType[]>(() =>
    Array.from({ length: 20 }, (_, index) => generateGridElement(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 5 }, (_, index) =>
      generateGridElement(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 5 }, (_, index) =>
      generateGridElement(currentLength + index)
    );
    setData((prev) => [...prev, ...newElements]);
  };

  return (
    <View style={styles.container}>
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderItem={({ item: element, index }) => (
          <GridElement element={element} index={index} />
        )}
      />
      <FloatingActionBar onPrepend={handlePrepend} onAppend={handleAppend} />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000000',
  },
  list: {
    flex: 1,
    backgroundColor: '#000000',
    height: 300,
  },
});
