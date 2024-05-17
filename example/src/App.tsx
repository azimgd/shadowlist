import * as React from 'react';
import {
  SafeAreaView,
  StyleSheet,
  View,
  Text,
  FlatList,
  Pressable,
} from 'react-native';
import { FlashList } from '@shopify/flash-list';
import ShadowListContainer from 'shadowlist';
import sample from './sample.json';

const CustomComponent = ({ item, index }: { item: any; index: number }) => {
  return (
    <View style={styles.item}>
      <Text style={styles.username}>
        {index} - {item.username}
      </Text>
      <Text style={styles.text}>{item.text}</Text>
    </View>
  );
};

const ListHeaderComponent = () => {
  return (
    <View style={[styles.item, styles.header]}>
      <Text style={styles.username}>Header</Text>
      <Text style={styles.text}>
        Lorem ipsum dolor sit amet consectetur adipisicing elit.
      </Text>
    </View>
  );
};

const ListFooterComponent = () => {
  return (
    <View style={[styles.item, styles.footer]}>
      <Text style={styles.username}>Footer</Text>
      <Text style={styles.text}>
        Lorem ipsum dolor sit amet consectetur adipisicing elit.
      </Text>
    </View>
  );
};

const ListEmptyComponent = () => {
  return (
    <View style={[styles.item, styles.empty]}>
      <Text style={styles.username}>Empty</Text>
    </View>
  );
};

/**
 * FlatList
 */
export const FlatListExample = ({ data }: { data: any[] }) => {
  return (
    <FlatList
      contentContainerStyle={styles.container}
      data={data}
      ListHeaderComponent={ListHeaderComponent}
      ListFooterComponent={ListFooterComponent}
      ListEmptyComponent={ListEmptyComponent}
      renderItem={({ item, index }) => (
        <CustomComponent item={item} index={index} />
      )}
    />
  );
};

/**
 * FlashList
 */
export const FlashListExample = ({ data }: { data: any[] }) => {
  return (
    <FlashList
      contentContainerStyle={styles.container}
      data={data}
      ListHeaderComponent={ListHeaderComponent}
      ListFooterComponent={ListFooterComponent}
      ListEmptyComponent={ListEmptyComponent}
      renderItem={({ item, index }) => (
        <CustomComponent item={item} index={index} />
      )}
      estimatedItemSize={200}
    />
  );
};

/**
 * ShadowList
 */
export const ShadowListExample = ({ data }: { data: any[] }) => {
  const shadowListContainerRef = React.useRef<{
    scrollToIndex: (index: number) => void;
    scrollToOffset: (offset: number) => void;
  }>(null);

  return (
    <ShadowListContainer
      contentContainerStyle={styles.container}
      ref={shadowListContainerRef}
      data={data}
      ListHeaderComponent={ListHeaderComponent}
      ListFooterComponent={ListFooterComponent}
      ListEmptyComponent={ListEmptyComponent}
      renderItem={({ item, index }) => (
        <CustomComponent item={item} index={index} />
      )}
      initialScrollIndex={100}
    />
  );
};

export default function App() {
  const [data, setData] = React.useState(Array(100).fill(sample).flat());

  const loadMore = React.useCallback(() => {
    setData((state) => state.concat(Array(50).fill(sample).flat()));
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <ShadowListExample data={data} />

      <Pressable style={[styles.item, styles.button]} onPress={loadMore}>
        <Text>Load more</Text>
      </Pressable>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#74b9ff',
  },
  item: {
    padding: 24,
    justifyContent: 'center',
    backgroundColor: '#0984e3',
    borderBottomColor: '#74b9ff',
    borderBottomWidth: 1,
  },
  header: {
    backgroundColor: '#0984e390',
  },
  footer: {
    backgroundColor: '#0984e390',
  },
  empty: {
    backgroundColor: '#0984e390',
  },
  username: {
    color: '#ffffff',
    fontWeight: '600',
    paddingBottom: 12,
  },
  text: {
    color: '#ffffff',
  },
  button: {
    backgroundColor: '#0984e390',
  },
});
