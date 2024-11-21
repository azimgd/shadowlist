import { useRef, useCallback } from 'react';
import { SafeAreaView, StyleSheet, Text, View } from 'react-native';
import type { DirectEventHandler } from 'react-native/Libraries/Types/CodegenTypes';
import {
  Shadowlist,
  type OnStartReached,
  type OnEndReached,
  type OnVisibleChange,
  type ShadowlistProps,
  type SLContainerRef,
} from 'shadowlist';
import useData from './useData';
import Element from './Element';

const IS_INVERTED = false;
const IS_HORIZONTAL = false;

const ListHeaderComponent = () => (
  <View style={styles.static}>
    <Text style={styles.text}>Header</Text>
  </View>
);

const ListFooterComponent = () => (
  <View style={styles.static}>
    <Text style={styles.text}>Footer</Text>
  </View>
);

const ListEmptyComponent = () => (
  <View style={styles.static}>
    <Text style={styles.text}>Footer</Text>
  </View>
);

const renderItem: ShadowlistProps['renderItem'] = ({ item, index }) => {
  return <Element item={item} index={index} />;
};

export default function App() {
  const data = useData({ length: 20, inverted: IS_INVERTED });
  const ref = useRef<SLContainerRef>(null);

  const onStartReached = useCallback<DirectEventHandler<OnStartReached>>(
    (event) => {
      !IS_INVERTED ? data.loadPrepend() : data.loadAppend();
      event.nativeEvent.distanceFromStart;
    },
    [data]
  );

  const onEndReached = useCallback<DirectEventHandler<OnEndReached>>(
    (event) => {
      !IS_INVERTED ? data.loadAppend() : data.loadPrepend();
      event.nativeEvent.distanceFromEnd;
    },
    [data]
  );

  const onVisibleChange = useCallback<DirectEventHandler<OnVisibleChange>>(
    (event) => {
      event.nativeEvent.visibleEndIndex;
    },
    []
  );

  return (
    <SafeAreaView style={styles.container}>
      <Shadowlist
        style={styles.container}
        ref={ref}
        renderItem={renderItem}
        data={data.data}
        keyExtractor={(item) => item.id}
        onVisibleChange={onVisibleChange}
        onEndReached={onEndReached}
        onStartReached={onStartReached}
        ListHeaderComponent={ListHeaderComponent}
        ListHeaderComponentStyle={styles.static}
        ListFooterComponent={ListFooterComponent}
        ListFooterComponentStyle={styles.static}
        ListEmptyComponent={ListEmptyComponent}
        ListEmptyComponentStyle={styles.static}
        inverted={IS_INVERTED}
        horizontal={IS_HORIZONTAL}
        initialScrollIndex={2}
      />
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#333333',
  },
  text: {
    color: '#333333',
    backgroundColor: '#1dd1a1',
    padding: 16,
  },
  static: {
    height: 120,
    backgroundColor: '#576574',
  },
});
