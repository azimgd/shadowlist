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
import Menu, { type VariantsKey } from './Menu';

const ITEMS_COUNT = 50;
const FINAL_SCROLL_INDEX = 0;

export default function HomeScreen() {
  const navigation = useNavigation();
  const shadowlistRef = useRef<SLContainerRef>(null);

  /**
   * Options are used to control the behavior of the list
   */
  const [options, setOptions] = useState({
    pressable: false,
    prepended: false,
    appended: false,
  });

  /**
   * Variants are used to control the type of the list
   */
  const [variants, setVariants] = useState<VariantsKey>('vertical-default');
  const inverted = variants.includes('inverted');
  const data = useData({
    length: ITEMS_COUNT,
    inverted,
  });

  useEffect(() => {
    if (!FINAL_SCROLL_INDEX) return;
    setTimeout(() => {
      shadowlistRef.current?.scrollToIndex({ index: FINAL_SCROLL_INDEX });
    }, 1000);
  }, []);

  useEffect(() => {
    navigation.setOptions({ title: `${variants}: ${data.data.length} items` });
  }, [variants, data.data.length, navigation]);

  /**
   * When header item is pressed
   */
  const handleHeaderItemPress = useCallback((key: OptionsKey) => {
    setOptions((state) => ({ ...state, [key]: !state[key] }));
  }, []);

  /**
   * When header item is pressed
   */
  const handleMenuItemPress = useCallback(
    (key: VariantsKey) => {
      setVariants(key);
      navigation.setOptions({ title: `${key}: ${data.data.length} items` });
    },
    [data.data.length, navigation]
  );

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

      !inverted ? data.loadPrepend() : data.loadAppend();
      event.nativeEvent.distanceFromStart;
    },
    [data, options.prepended, inverted]
  );

  /**
   * When end is reached
   */
  const onEndReached = useCallback<DirectEventHandler<OnEndReached>>(
    (event) => {
      if (!options.appended) return;

      !inverted ? data.loadAppend() : data.loadPrepend();
      event.nativeEvent.distanceFromEnd;
    },
    [data, options.appended, inverted]
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

  const listProps = {
    templates: {
      ListTemplateComponentUniqueIdYarrow: templateYarrow,
      ListTemplateComponentUniqueIdRobin: templateRobin,
    },
    data: data.data,
    onVisibleChange: onVisibleChange,
    onStartReached: onStartReached,
    onEndReached: onEndReached,
    onScroll: onScroll,
    ListHeaderComponent: ListHeaderComponent,
    ListFooterComponent: ListFooterComponent,
    ListFooterComponentStyle: styles.static,
    ListEmptyComponent: ListEmptyComponent,
    ListEmptyComponentStyle: styles.static,
  };

  return (
    <View style={styles.container}>
      {variants === 'vertical-default' && (
        <Shadowlist
          {...listProps}
          ref={shadowlistRef}
          inverted={false}
          horizontal={false}
        />
      )}

      {variants === 'vertical-inverted' && (
        <Shadowlist
          {...listProps}
          ref={shadowlistRef}
          inverted={true}
          horizontal={false}
        />
      )}

      {variants === 'horizontal-default' && (
        <Shadowlist
          {...listProps}
          ref={shadowlistRef}
          inverted={false}
          horizontal={true}
        />
      )}

      {variants === 'horizontal-inverted' && (
        <Shadowlist
          {...listProps}
          ref={shadowlistRef}
          inverted={true}
          horizontal={true}
        />
      )}

      <Menu onPress={handleMenuItemPress} />
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
