import { useRef, useCallback, useEffect, useState } from 'react';
import { View, StyleSheet, Text } from 'react-native';
import type { DirectEventHandler } from 'react-native/Libraries/Types/CodegenTypes';
import {
  Shadowlist,
  type OnStartReached,
  type OnEndReached,
  type OnVisibleChange,
  type OnScroll,
  type SLContainerRef,
} from 'shadowlist';
import useData from './useData';
import Header, { type OptionsKey } from './Header';
import Element from './Element';
import { useNavigation } from '@react-navigation/native';

const ITEMS_COUNT = 50;
const IS_INVERTED = false;
const IS_HORIZONTAL = false;
const INITIAL_SCROLL_INDEX = 0;
const FINAL_SCROLL_INDEX = 0;

export default function HomeScreen() {
  const navigation = useNavigation();
  const data = useData({ length: ITEMS_COUNT, inverted: IS_INVERTED });
  const ref = useRef<SLContainerRef>(null);
  const [options, setOptions] = useState({
    pressable: false,
    prepended: false,
    appended: false,
  });

  useEffect(() => {
    if (!FINAL_SCROLL_INDEX) return;
    setTimeout(() => {
      ref.current?.scrollToIndex({ index: FINAL_SCROLL_INDEX });
    }, 1000);
  }, []);

  useEffect(() => {
    navigation.setOptions({ title: `shadowlist: ${data.data.length} items` });
  }, [data.data.length, navigation]);

  /**
   * When header item is pressed
   */
  const handleHeaderItemPress = useCallback((key: OptionsKey) => {
    setOptions((state) => ({ ...state, [key]: !state[key] }));
  }, []);

  /**
   * When element item is pressed
   */
  const handleElementItemPress = useCallback(
    (index: number) => {
      if (!options.pressable) return;
      data.update(index);
    },
    [data, options]
  );

  /**
   * Header component
   */
  const ListHeaderComponent = useCallback(
    () => <Header options={options} onPress={handleHeaderItemPress} />,
    [handleHeaderItemPress, options]
  );
  const ListFooterComponent = useCallback(
    () => <Text style={styles.text}>Footer</Text>,
    []
  );
  const ListEmptyComponent = useCallback(
    () => <Text style={styles.text}>Empty</Text>,
    []
  );

  /**
   * When start is reached
   */
  const onStartReached = useCallback<DirectEventHandler<OnStartReached>>(
    (event) => {
      if (!options.prepended) return;

      !IS_INVERTED ? data.loadPrepend() : data.loadAppend();
      event.nativeEvent.distanceFromStart;
    },
    [data, options.prepended]
  );

  /**
   * When end is reached
   */
  const onEndReached = useCallback<DirectEventHandler<OnEndReached>>(
    (event) => {
      if (!options.appended) return;

      !IS_INVERTED ? data.loadAppend() : data.loadPrepend();
      event.nativeEvent.distanceFromEnd;
    },
    [data, options.appended]
  );

  /**
   * When visible items change
   */
  const onVisibleChange = useCallback<DirectEventHandler<OnVisibleChange>>(
    (event) => {
      event.nativeEvent.visibleEndIndex;
    },
    []
  );

  /**
   * When scroll
   */
  const onScroll = useCallback<DirectEventHandler<OnScroll>>((event) => {
    event.nativeEvent.contentOffset.y;
  }, []);

  /**
   * Element template: Yarrow
   */
  const templateYarrow = useCallback(
    () => <Element onPress={handleElementItemPress} data={data.data} />,
    [data.data, handleElementItemPress]
  );
  const templateRobin = useCallback(
    () => <Element onPress={handleElementItemPress} data={data.data} />,
    [data.data, handleElementItemPress]
  );

  return (
    <View style={styles.container}>
      <Shadowlist
        ref={ref}
        templates={{
          ListTemplateComponentUniqueIdYarrow: templateYarrow,
          ListTemplateComponentUniqueIdRobin: templateRobin,
        }}
        data={data.data}
        onVisibleChange={onVisibleChange}
        onStartReached={onStartReached}
        onEndReached={onEndReached}
        onScroll={onScroll}
        ListHeaderComponent={ListHeaderComponent}
        ListFooterComponent={ListFooterComponent}
        ListFooterComponentStyle={styles.static}
        ListEmptyComponent={ListEmptyComponent}
        ListEmptyComponentStyle={styles.static}
        inverted={IS_INVERTED}
        horizontal={IS_HORIZONTAL}
        initialScrollIndex={INITIAL_SCROLL_INDEX}
        numColumns={1}
        contentContainerStyle={styles.list}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#333333',
  },
  list: {
    flex: 1,
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
