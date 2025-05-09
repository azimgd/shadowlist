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
  type ShadowlistProps,
  type SLContainerNativeProps,
} from 'shadowlist';
import useData from './useData';
import Header, { type OptionsKey } from './Header';
import ElementVertical, { ElementVerticalDynamic } from './ElementVertical';
import ElementGridMasonry from './ElementGridMasonry';
import ElementGridAligned from './ElementGridAligned';
import ElementLine from './ElementLine';
import { useNavigation } from '@react-navigation/native';
import Menu, { type VariantsKey } from './Menu';
import HeaderInput from './HeaderInput';

const ITEMS_COUNT = 200;

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
      if (key === 'scroll-to-index') {
        shadowlistRef.current?.scrollToIndex({ index: 10, animated: true });
        return;
      }

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
  const ListHeaderInputComponent = useCallback(
    () => <HeaderInput options={options} onPress={handleHeaderItemPress} />,
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
  const templateVerticalYarrow = useCallback(
    () => <ElementVertical onPress={handleElementItemPress} data={data.data} />,
    [data.data, handleElementItemPress]
  );
  const templateVerticalRobin = useCallback(
    () => <ElementVertical onPress={handleElementItemPress} data={data.data} />,
    [data.data, handleElementItemPress]
  );
  const templateGridMasonryYarrow = useCallback(
    () => (
      <ElementGridMasonry onPress={handleElementItemPress} data={data.data} />
    ),
    [data.data, handleElementItemPress]
  );
  const templateGridMasonryRobin = useCallback(
    () => (
      <ElementGridMasonry onPress={handleElementItemPress} data={data.data} />
    ),
    [data.data, handleElementItemPress]
  );
  const templateGridAlignedYarrow = useCallback(
    () => (
      <ElementGridAligned onPress={handleElementItemPress} data={data.data} />
    ),
    [data.data, handleElementItemPress]
  );
  const templateGridAlignedRobin = useCallback(
    () => (
      <ElementGridAligned onPress={handleElementItemPress} data={data.data} />
    ),
    [data.data, handleElementItemPress]
  );
  const templateLineYarrow = useCallback(
    () => <ElementLine onPress={handleElementItemPress} data={data.data} />,
    [data.data, handleElementItemPress]
  );
  const templateLineRobin = useCallback(
    () => <ElementLine onPress={handleElementItemPress} data={data.data} />,
    [data.data, handleElementItemPress]
  );

  const listProps: Omit<SLContainerNativeProps, 'data'> & ShadowlistProps = {
    ref: shadowlistRef,
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
    renderItem: ({ item }) => <ElementVerticalDynamic item={item} />,
  };

  return (
    <View style={styles.container}>
      {variants === 'vertical-default' && (
        <Shadowlist
          {...listProps}
          templates={{
            ListTemplateComponentUniqueIdYarrow: templateVerticalYarrow,
            ListTemplateComponentUniqueIdRobin: templateVerticalRobin,
          }}
        />
      )}

      {variants === 'vertical-inverted' && (
        <Shadowlist
          {...listProps}
          templates={{
            ListTemplateComponentUniqueIdYarrow: templateVerticalYarrow,
            ListTemplateComponentUniqueIdRobin: templateVerticalRobin,
          }}
          inverted={true}
          horizontal={false}
        />
      )}

      {variants === 'horizontal-default' && (
        <Shadowlist
          {...listProps}
          templates={{
            ListTemplateComponentUniqueIdYarrow: templateVerticalYarrow,
            ListTemplateComponentUniqueIdRobin: templateVerticalRobin,
          }}
          inverted={false}
          horizontal={true}
        />
      )}

      {variants === 'horizontal-inverted' && (
        <Shadowlist
          {...listProps}
          templates={{
            ListTemplateComponentUniqueIdYarrow: templateVerticalYarrow,
            ListTemplateComponentUniqueIdRobin: templateVerticalRobin,
          }}
          inverted={true}
          horizontal={true}
        />
      )}

      {variants === 'grid-masonry-3' && (
        <Shadowlist
          {...listProps}
          templates={{
            ListTemplateComponentUniqueIdYarrow: templateGridMasonryYarrow,
            ListTemplateComponentUniqueIdRobin: templateGridMasonryRobin,
          }}
          numColumns={3}
          ListHeaderComponentStyle={{ paddingBottom: 12 }}
        />
      )}

      {variants === 'grid-aligned-3' && (
        <Shadowlist
          {...listProps}
          templates={{
            ListTemplateComponentUniqueIdYarrow: templateGridAlignedYarrow,
            ListTemplateComponentUniqueIdRobin: templateGridAlignedRobin,
          }}
          numColumns={3}
        />
      )}

      {variants === 'chatui' && (
        <Shadowlist
          {...listProps}
          ListHeaderComponent={ListHeaderInputComponent}
          templates={{
            ListTemplateComponentUniqueIdYarrow: templateLineYarrow,
            ListTemplateComponentUniqueIdRobin: templateLineRobin,
          }}
          inverted={true}
        />
      )}

      {variants === 'initial-scroll-index' && (
        <Shadowlist
          {...listProps}
          templates={{
            ListTemplateComponentUniqueIdYarrow: templateVerticalYarrow,
            ListTemplateComponentUniqueIdRobin: templateVerticalRobin,
          }}
          initialScrollIndex={10}
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
