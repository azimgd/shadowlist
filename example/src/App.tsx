import * as React from 'react';

import { StyleSheet, View, Text } from 'react-native';
import { CraigsListContainer } from 'react-native-craigs-list';

export default function App() {
  return (
    <CraigsListContainer style={styles.container}>
      {Array.from(Array(100).keys()).map((item) => (
        <View key={item} style={styles.item}>
          <Text>item → {item}</Text>
        </View>
      ))}
    </CraigsListContainer>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  item: {
    height: 150,
    paddingHorizontal: 40,
    justifyContent: 'center',
    borderBottomColor: '#eeeeee',
    borderBottomWidth: 1,
  },
});
