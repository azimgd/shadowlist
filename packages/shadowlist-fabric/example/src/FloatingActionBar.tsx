import { View, TouchableOpacity, Text, StyleSheet } from 'react-native';

interface FloatingActionBarProps {
  onPrepend: () => void;
  onAppend: () => void;
}

export const FloatingActionBar = ({ onPrepend, onAppend }: FloatingActionBarProps) => {
  return (
    <View style={styles.container}>
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
    bottom: 40,
    right: 20,
    flexDirection: 'column',
    gap: 12,
  },
  button: {
    width: 56,
    height: 56,
    borderRadius: 28,
    backgroundColor: '#1d9bf0',
    justifyContent: 'center',
    alignItems: 'center',
    shadowColor: '#000',
    shadowOffset: {
      width: 0,
      height: 2,
    },
    shadowOpacity: 0.25,
    shadowRadius: 3.84,
    elevation: 5,
  },
  buttonText: {
    fontSize: 24,
    color: '#ffffff',
    fontWeight: 'bold',
  },
});
