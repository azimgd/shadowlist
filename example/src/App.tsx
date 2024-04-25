import * as React from 'react';

import { StyleSheet, Text } from 'react-native';
import { CraigsListContainer, CraigsListItem } from 'react-native-craigs-list';

export default function App() {
  return (
    <CraigsListContainer style={styles.container}>
      {Array.from(Array(100).keys()).map((item) => (
        <CraigsListItem key={item} style={styles.item}>
          <Text>item → {item}</Text>
        </CraigsListItem>
      ))}
    </CraigsListContainer>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  item: {
    height: 50,
    paddingHorizontal: 40,
    justifyContent: 'center',
    borderBottomColor: '#eeeeee',
    borderBottomWidth: 1,
  },
});
