import { useState, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { Shadowlist, type ShadowListCommands } from 'react-native-shadowlist';
import { HeavyListItem } from './HeavyListItem';
import { FloatingActionBar } from './FloatingActionBar';

const LOREM_WORDS = [
  'Lorem', 'ipsum', 'dolor', 'sit', 'amet', 'consectetur', 'adipiscing', 'elit',
  'sed', 'do', 'eiusmod', 'tempor', 'incididunt', 'ut', 'labore', 'et', 'dolore',
  'magna', 'aliqua', 'Ut', 'enim', 'ad', 'minim', 'veniam', 'quis', 'nostrud',
  'exercitation', 'ullamco', 'laboris', 'nisi', 'aliquip', 'ex', 'ea', 'commodo',
  'consequat', 'Duis', 'aute', 'irure', 'in', 'reprehenderit', 'voluptate', 'velit',
  'esse', 'cillum', 'fugiat', 'nulla', 'pariatur', 'Excepteur', 'sint', 'occaecat',
];

function generateUniqueId(): string {
  return `${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
}

function generateRandomText(seed: number): string {
  const wordCount = 20 + (seed % 40); // 20-60 words
  const words: string[] = [];

  for (let i = 0; i < wordCount; i++) {
    const index = (seed + i) % LOREM_WORDS.length;
    words.push(LOREM_WORDS[index]!);
  }

  return words.join(' ') + '.';
}

export default function App() {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const [data, setData] = useState<{id: string, text: string}[]>(() =>
    Array.from({ length: 100 }, (_, index) => ({
      id: generateUniqueId(),
      text: generateRandomText(index),
    }))
  );

  const handlePrepend = () => {
    const newItems = Array.from({ length: 10 }, (_, index) => ({
      id: generateUniqueId(),
      text: generateRandomText(index),
    }));
    setData((prev) => [...newItems, ...prev]);
    shadowlistRef.current?.prependElements(newItems.length);
  };

  const handleAppend = () => {
    const newItems = Array.from({ length: 10 }, (_, index) => ({
      id: generateUniqueId(),
      text: generateRandomText(index),
    }));
    setData((prev) => [...prev, ...newItems]);
    shadowlistRef.current?.appendElements(newItems.length);
  };

  return (
    <View style={styles.container}>
      <Shadowlist
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderItem={({ item, index }) => (
          <HeavyListItem id={item.id} index={index} text={item.text} />
        )}
      />
      <FloatingActionBar onPrepend={handlePrepend} onAppend={handleAppend} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#333333',
  },
  list: {
    flex: 1,
    backgroundColor: '#333333',
  },
});
