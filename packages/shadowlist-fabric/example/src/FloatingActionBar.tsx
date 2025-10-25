import { View, TouchableOpacity, Text, StyleSheet } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';

interface FloatingActionBarProps {
  onPrepend: () => void;
  onAppend: () => void;
}

export const FloatingActionBar = ({ onPrepend, onAppend }: FloatingActionBarProps) => {
  const insets = useSafeAreaInsets();

  return (
    <View style={[styles.container, { top: insets.top }]}>
      <TouchableOpacity style={styles.button} onPress={onPrepend}>
        <Text style={styles.buttonText}>↑</Text>
      </TouchableOpacity>
      <TouchableOpacity style={styles.button} onPress={onAppend}>
        <Text style={styles.buttonText}>↓</Text>
      </TouchableOpacity>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    position: 'absolute',
    right: 12,
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
  buttonText: {
    color: '#ffffff',
    fontSize: 16,
    fontWeight: '600',
  },
});
