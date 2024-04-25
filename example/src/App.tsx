import * as React from 'react';

import { SafeAreaView, StyleSheet, Text } from 'react-native';
import { CraigsListContainer, CraigsListItem } from 'react-native-craigs-list';

export default function App() {
  return (
    <SafeAreaView style={styles.container}>
      <CraigsListContainer style={styles.container}>
        {Array.from(Array(100).keys()).map((item) => (
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
