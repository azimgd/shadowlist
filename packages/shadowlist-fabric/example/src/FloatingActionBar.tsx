import { View, TouchableOpacity, Text, StyleSheet } from 'react-native';
import { useState } from 'react';

interface FloatingActionBarProps {
  onPrepend: () => void;
  onAppend: () => void;
  onScrollToIndex: (index: number) => void;
  dataLength: number;
}

export const FloatingActionBar = ({ onPrepend, onAppend, onScrollToIndex, dataLength }: FloatingActionBarProps) => {
  const [targetIndex, setTargetIndex] = useState<number | null>(null);

  const handleScrollToRandom = () => {
    const randomIndex = Math.floor(Math.random() * dataLength);
    setTargetIndex(randomIndex);
    onScrollToIndex(randomIndex);
  };

  return (
    <View style={styles.container}>
      <TouchableOpacity style={styles.button} onPress={onPrepend}>
        <Text style={styles.buttonText}>↑</Text>
      </TouchableOpacity>
      <TouchableOpacity style={styles.button} onPress={onAppend}>
        <Text style={styles.buttonText}>↓</Text>
      </TouchableOpacity>
      <TouchableOpacity style={styles.scrollButton} onPress={handleScrollToRandom}>
        <Text style={styles.buttonText}>{targetIndex ?? 'rnd'}</Text>
      </TouchableOpacity>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    position: 'absolute',
    top: 16,
    right: 16,
    flexDirection: 'row',
    gap: 8,
    backgroundColor: 'rgba(28, 28, 30, 0.8)',
    borderRadius: 18,
    padding: 4,
  },
  button: {
    width: 32,
    height: 32,
    borderRadius: 16,
    backgroundColor: '#FF9500',
    alignItems: 'center',
    justifyContent: 'center',
  },
  scrollButton: {
    minWidth: 32,
    height: 32,
    borderRadius: 16,
    backgroundColor: '#34C759',
    alignItems: 'center',
    justifyContent: 'center',
    paddingHorizontal: 8,
  },
  buttonText: {
    color: '#ffffff',
    fontSize: 14,
    fontWeight: '500',
  },
});
