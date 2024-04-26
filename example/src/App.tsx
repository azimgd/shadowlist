import * as React from 'react';

import { SafeAreaView, StyleSheet, Text, FlatList } from 'react-native';
import { CraigsListContainer, CraigsListItem } from 'react-native-craigs-list';
import data from './data.json';

const chats = Array(100).fill(data).flat();

const CustomComponent = ({ item, index }: { item: any; index: number }) => {
  const [customState, setCustomState] = React.useState(0);

  React.useEffect(() => {
    setInterval(() => {
      setCustomState(() => Math.random());
    }, 1000);
  }, []);

  return (
    <CraigsListItem key={index} style={styles.item}>
      <Text style={styles.username}>
        {index} - {item.username}
      </Text>
      <Text style={styles.text}>{item.text}</Text>
      <Text style={styles.text}>{customState}</Text>
    </CraigsListItem>
  );
};

/**
 * FlatList
 */
export const FlatListExample = () => {
  return (
    <FlatList
      style={styles.container}
      data={chats}
      renderItem={({ item, index }) => (
        <CustomComponent item={item} index={index} />
      )}
    />
  );
};

/**
 * CraigsList
 */
export const CraigsListExample = () => {
  const craigsListContainerRef = React.useRef<{
    scrollToIndex: (index: number) => void;
  }>(null);

  return (
    <CraigsListContainer style={styles.container} ref={craigsListContainerRef}>
      {chats.map((item, index) => (
        <CustomComponent item={item} index={index} />
      ))}
    </CraigsListContainer>
  );
};

export default function App() {
  return (
    <SafeAreaView style={styles.container}>
      <CraigsListExample />
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
  username: {
    color: '#ffffff',
    fontWeight: '600',
    paddingBottom: 12,
  },
  text: {
    color: '#ffffff',
  },
});
