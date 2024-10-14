import { SafeAreaView, StyleSheet, Text } from 'react-native';
import { ShadowlistView } from 'shadowlist';

export default function App() {
  return (
    <SafeAreaView style={styles.container}>
      <ShadowlistView style={styles.content}>
        {Array.from({ length: 10000 }, (_, i) => (
          <Text style={styles.text} key={i}>
            {i}
          </Text>
        ))}
      </ShadowlistView>
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
    backgroundColor: 'red',
  },
  text: {
    color: 'white',
  },
});
