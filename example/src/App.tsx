import * as React from 'react';

import {
  SafeAreaView,
  StyleSheet,
  View,
  Text,
  FlatList,
  Pressable,
} from 'react-native';
import ShadowListContainer from 'react-native-shadow-list';
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
    <View style={styles.header}>
      <Text style={styles.username}>Header</Text>
      <Text style={styles.text}>
        Lorem ipsum dolor sit amet consectetur adipisicing elit.
      </Text>
    </View>
  );
};

const ListFooterComponent = () => {
  return (
    <View style={styles.header}>
      <Text style={styles.username}>Footer</Text>
      <Text style={styles.text}>
        Lorem ipsum dolor sit amet consectetur adipisicing elit.
      </Text>
    </View>
  );
};

/**
 * FlatList
 */
export const FlatListExample = ({ data }: { data: any[] }) => {
  return (
    <FlatList
      style={styles.container}
      data={data}
      ListHeaderComponent={ListHeaderComponent}
      ListFooterComponent={ListFooterComponent}
      renderItem={({ item, index }) => (
        <CustomComponent item={item} index={index} />
      )}
    />
  );
};

/**
 * ShadowList
 */
export const ShadowListExample = ({ data }: { data: any[] }) => {
  const shadowListContainerRef = React.useRef<{
    scrollToIndex: (index: number) => void;
  }>(null);

  return (
    <ShadowListContainer
      style={styles.container}
      ref={shadowListContainerRef}
      data={data}
      ListHeaderComponent={ListHeaderComponent}
      ListFooterComponent={ListFooterComponent}
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
    <SafeAreaView style={styles.container}>
      <ShadowListExample data={data} />

      <Pressable style={styles.button} onPress={loadMore}>
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
    padding: 24,
    justifyContent: 'center',
    backgroundColor: '#0984e3',
    borderBottomColor: '#74b9ff',
    borderBottomWidth: 1,
  },
  footer: {
    padding: 24,
    justifyContent: 'center',
    backgroundColor: '#0984e3',
    borderBottomColor: '#74b9ff',
    borderBottomWidth: 1,
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
    padding: 24,
    justifyContent: 'center',
    backgroundColor: '#0984e3',
    borderBottomColor: '#74b9ff',
    borderBottomWidth: 1,
  },
});
