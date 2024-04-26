import * as React from 'react';

import { SafeAreaView, StyleSheet, Text } from 'react-native';
import { CraigsListContainer, CraigsListItem } from 'react-native-craigs-list';

export default function App() {
  const craigsListContainerRef = React.useRef<{
    scrollToIndex: (index: number) => void;
  }>(null);

  React.useEffect(() => {
    setTimeout(() => {
      craigsListContainerRef.current?.scrollToIndex(800);
    }, 5000);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <CraigsListContainer
        style={styles.container}
        ref={craigsListContainerRef}
      >
        {Array.from(Array(1000).keys()).map((item) => (
          <CraigsListItem key={item} style={styles.item}>
            <Text style={styles.text}>item → {item}</Text>
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
    height: 50,
    paddingHorizontal: 40,
    justifyContent: 'center',
    backgroundColor: '#0984e3',
    borderBottomColor: '#74b9ff',
    borderBottomWidth: 1,
  },
  text: {
    color: '#ffffff',
    fontWeight: '600',
  },
});
