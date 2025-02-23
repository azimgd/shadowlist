import { StyleSheet, TextInput, View } from 'react-native';
import Header from './Header';

export type OptionsKey = 'prepended' | 'appended' | 'pressable';

export type HeaderInputProps = {
  options: Record<OptionsKey, boolean>;
  onPress: (optionsKey: OptionsKey) => void;
};

const HeaderInput = (props: HeaderInputProps) => {
  return (
    <View style={styles.container}>
      <Header {...props} />
      <TextInput style={styles.input} value="Enter your message" />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    borderBottomColor: '#dddddd20',
    borderBottomWidth: StyleSheet.hairlineWidth,
    marginBottom: 32,
  },
  input: {
    backgroundColor: '#dddddd20',
    color: '#ffffff',
    height: 32,
    width: '100%',
  },
});

export default HeaderInput;
