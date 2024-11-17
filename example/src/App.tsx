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

const IS_INVERTED = true;
const IS_HORIZONTAL = false;

const ListHeaderComponent = () => <Text style={styles.text}>Header</Text>;
const ListFooterComponent = () => <Text style={styles.text}>Footer</Text>;

const renderItem: ShadowlistProps['renderItem'] = ({ item, index }) => {
  return (
    <Text style={styles.text} key={index}>
      {index} - {item.id} - {item.text}
    </Text>
  );
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
    backgroundColor: 'black',
  },
  content: {
    flex: 1,
  },
  text: {
    color: 'white',
    padding: 16,
    borderBottomColor: '#333333',
    borderBottomWidth: 1,
  },
});
