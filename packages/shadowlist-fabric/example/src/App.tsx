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
  const [data, setData] = useState<{index: number, text: string}[]>(() =>
    Array.from({ length: 1000 }, (_, index) => ({
      index: index,
      text: generateRandomText(index),
    }))
  );

  const handlePrepend = () => {
    const newItem = { index: -1, text: generateRandomText(10) };
    setData((prev) => [newItem, ...prev]);
    shadowlistRef.current?.prependElements(1);
  };

  const handleAppend = () => {
    const newItem = { index: 1001, text: generateRandomText(10) };
    setData((prev) => [...prev, newItem]);
    shadowlistRef.current?.appendElements(1);
  };

  return (
    <View style={styles.container}>
      <Shadowlist
        ref={shadowlistRef}
        renderItem={({ index }) => (
          <HeavyListItem index={data[index]?.index!} text={(data[index]?.text || data[0]?.text)!} />
        )}
      />
      <FloatingActionBar onPrepend={handlePrepend} onAppend={handleAppend} />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
});
