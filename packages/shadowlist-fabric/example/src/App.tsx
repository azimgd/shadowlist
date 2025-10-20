import { View, StyleSheet } from 'react-native';
import { Shadowlist } from 'react-native-shadowlist';
import { HeavyListItem } from './HeavyListItem';

export default function App() {
  return (
    <View style={styles.container}>
      <Shadowlist
        renderItem={({ index }) => <HeavyListItem index={index} />}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
});
