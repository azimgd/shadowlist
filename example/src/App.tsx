import { useRef, useCallback } from 'react';
import { SafeAreaView, StyleSheet, Text } from 'react-native';
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

const ListHeaderComponent = () => <Text style={styles.text}>Header</Text>;
const ListFooterComponent = () => <Text style={styles.text}>Footer</Text>;

const renderItem: ShadowlistProps['renderItem'] = ({ item, index }) => {
  return <Element item={item} index={index} />;
};

export default function App() {
  const data = useData({ length: 20, inverted: IS_INVERTED });
  const ref = useRef<SLContainerRef>(null);
  const onStartReached = useCallback<DirectEventHandler<OnStartReached>>(
    (event) => {
      !IS_INVERTED ? data.loadPrepend() : data.loadAppend();
      console.debug('onStartReached', event.nativeEvent.distanceFromStart);
    },
    // eslint-disable-next-line react-hooks/exhaustive-deps
    []
  );

  const onEndReached = useCallback<DirectEventHandler<OnEndReached>>(
    (event) => {
      !IS_INVERTED ? data.loadAppend() : data.loadPrepend();
      console.debug('onEndReached', event.nativeEvent.distanceFromEnd);
    },
    // eslint-disable-next-line react-hooks/exhaustive-deps
    []
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
        ListFooterComponent={ListFooterComponent}
        inverted={IS_INVERTED}
        horizontal={IS_HORIZONTAL}
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
  },
});
