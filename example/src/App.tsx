import { SafeAreaView, StyleSheet, Text } from 'react-native';
import { SLContainer } from 'shadowlist';

export default function App() {
  return (
    <SafeAreaView style={styles.container}>
      <SLContainer style={styles.content} horizontal>
        {Array.from({ length: 10 }, (_, i) => (
          <Text style={styles.text} key={i}>
            {i} Lorem Ipsum is simply dummy text of the printing and typesetting
            industry. Lorem Ipsum has been the industry's standard dummy text
            ever since the 1500s, when an unknown printer took a galley of type
            and scrambled it to make a type specimen book. It has survived not
            only five centuries, but also the leap into electronic typesetting,
            remaining essentially unchanged. It was popularised in the 1960s
            with the release of Letraset sheets containing Lorem Ipsum passages,
            and more recently with desktop publishing software like Aldus
            PageMaker including versions of Lorem Ipsum.
          </Text>
        ))}
      </SLContainer>
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
    height: 100,
    width: 100,
    color: 'white',
    padding: 16,
    borderBottomColor: '#333333',
    borderBottomWidth: 1,
  },
});
