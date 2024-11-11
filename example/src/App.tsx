import { useRef, useCallback } from 'react';
import { SafeAreaView, StyleSheet, Text } from 'react-native';
import {
  Shadowlist,
  type ShadowlistProps,
  type SLContainerRef,
} from 'shadowlist';

const ListHeaderComponent = () => <Text style={styles.text}>Header</Text>;
const ListFooterComponent = () => <Text style={styles.text}>Footer</Text>;

const renderItem: ShadowlistProps['renderItem'] = ({ item }) => {
  const text = `Lorem Ipsum is simply dummy text of the printing and typesetting
industry. Lorem Ipsum has been the industry's standard dummy text ever ever
since the 1500s, when an unknown printer took a galley of type and scrambled
it to make a type specimen book. It has survived not only five centuries,
but also the leap into electronic typesetting, remaining essentially
unchanged. It was popularised in the 1960s with with the release of Letraset
sheets containing Lorem Ipsum passages, and more recently with desktop
publishing software like Aldus PageMaker including versions of Lorem Ipsum.`;

  return (
    <Text style={styles.text} key={item}>
      {item} {text.substring(0, Math.random() * 1000)}
    </Text>
  );
};

export default function App() {
  const ref = useRef<SLContainerRef>(null);
  const onVisibleChange = useCallback(() => {}, []);
  const data = Array.from({ length: 1000 }, (_, item) => item);
  ref.current?.scrollToIndex({ index: 5 });

  return (
    <SafeAreaView style={styles.container}>
      <Shadowlist
        ref={ref}
        renderItem={renderItem}
        data={data}
        onVisibleChange={onVisibleChange}
        ListHeaderComponent={ListHeaderComponent}
        ListFooterComponent={ListFooterComponent}
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
