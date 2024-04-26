import * as React from 'react';

import { SafeAreaView, StyleSheet, Text } from 'react-native';
import { CraigsListContainer, CraigsListItem } from 'react-native-craigs-list';
import data from './data.json';

const chats = Array(800).fill(data).flat();

export default function App() {
  const craigsListContainerRef = React.useRef<{
    scrollToIndex: (index: number) => void;
  }>(null);

  return (
    <SafeAreaView style={styles.container}>
      <CraigsListContainer
        style={styles.container}
        ref={craigsListContainerRef}
      >
        {chats.map((item, index) => (
          <CraigsListItem key={index} style={styles.item}>
            <Text style={styles.username}>{item.username}</Text>
            <Text style={styles.text}>{item.text}</Text>
          </CraigsListItem>
        ))}
      </CraigsListContainer>
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
