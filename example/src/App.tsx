import * as React from 'react';
import {
  SafeAreaView,
  StyleSheet,
  View,
  Text,
  FlatList,
  Pressable,
  TouchableOpacity,
} from 'react-native';
import { FlashList } from '@shopify/flash-list';
import ShadowList, {
  type ScrollToIndexOptions,
  type ScrollToOffsetOptions,
} from 'shadowlist';
import sample from './sample.json';

const CustomComponent = ({ item, index }: { item: any; index: number }) => {
  return (
    <TouchableOpacity style={styles.item}>
      <Text style={styles.username}>
        {index} - {item.username}
      </Text>
      <Text style={styles.text}>{item.text}</Text>
    </TouchableOpacity>
  );
};

const ListHeaderComponent = () => {
  return (
    <View style={[styles.item, styles.header]}>
      <Text style={styles.username}>Header</Text>

      <ShadowList
        contentContainerStyle={styles.container}
        data={sample}
        renderItem={({ item, index }) => (
          <CustomComponent item={item} index={index} />
        )}
        horizontal
      />
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
  const shadowListRef = React.useRef<{
    scrollToIndex: (options: ScrollToIndexOptions) => void;
    scrollToOffset: (offset: ScrollToOffsetOptions) => void;
  }>(null);

  return (
    <ShadowList
      contentContainerStyle={styles.container}
      ref={shadowListRef}
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

export default function App() {
  const [data, setData] = React.useState(Array(100).fill(sample).flat());

  const loadMore = React.useCallback(() => {
    setData((state) => state.concat(Array(50).fill(sample).flat()));
  }, []);

  return (
    <SafeAreaView style={styles.safeareaview}>
      <ShadowListExample data={data} />

      <Pressable style={[styles.item, styles.button]} onPress={loadMore}>
        <Text>Load more</Text>
      </Pressable>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeareaview: {
    flex: 1,
  },
  container: {
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
    height: 200,
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
