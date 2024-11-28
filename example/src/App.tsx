import { useRef, useCallback, useEffect } from 'react';
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

const ITEMS_COUNT = 500;
const IS_INVERTED = false;
const IS_HORIZONTAL = false;
const INITIAL_SCROLL_INDEX = 0;
const FINAL_SCROLL_INDEX = 0;

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
  const data = useData({ length: ITEMS_COUNT, inverted: IS_INVERTED });
  const ref = useRef<SLContainerRef>(null);

  useEffect(() => {
    if (!FINAL_SCROLL_INDEX) return;
    setTimeout(() => {
      ref.current?.scrollToIndex({ index: FINAL_SCROLL_INDEX });
    }, 1000);
  }, []);

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
        initialScrollIndex={INITIAL_SCROLL_INDEX}
        contentContainerStyle={styles.container}
        virtualizedPaginationEnabled
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
