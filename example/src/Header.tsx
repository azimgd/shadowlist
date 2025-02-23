import { StyleSheet, Text, View, Pressable } from 'react-native';

export type OptionsKey = 'prepended' | 'appended' | 'pressable';

export type HeaderProps = {
  options: Record<OptionsKey, boolean>;
  onPress: (optionsKey: OptionsKey) => void;
};

const Header = (props: HeaderProps) => {
  const handlePress = (optionsKey: OptionsKey) => () => {
    props.onPress(optionsKey);
  };

  const pressableStyle = props.options.pressable
    ? styles.itemActive
    : styles.itemInactive;
  const prependedStyle = props.options.prepended
    ? styles.itemActive
    : styles.itemInactive;
  const appendedStyle = props.options.appended
    ? styles.itemActive
    : styles.itemInactive;

  return (
    <View style={styles.container}>
      <Pressable
        style={[styles.item, prependedStyle]}
        onPress={handlePress('prepended')}
      >
        <Text style={styles.title}>prepended</Text>
      </Pressable>
      <Pressable
        style={[styles.item, appendedStyle]}
        onPress={handlePress('appended')}
      >
        <Text style={styles.title}>appended</Text>
      </Pressable>
      <Pressable
        style={[styles.item, pressableStyle]}
        onPress={handlePress('pressable')}
      >
        <Text style={styles.title}>pressable</Text>
      </Pressable>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    padding: 16,
    flexDirection: 'row',
    gap: 16,
    borderBottomColor: '#dddddd20',
    borderBottomWidth: StyleSheet.hairlineWidth,
  },
  item: {
    backgroundColor: '#dddddd20',
    borderWidth: 1,
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 16,
  },
  itemActive: {
    borderColor: '#1dd1a1',
  },
  itemInactive: {
    borderColor: '#dddddd20',
  },
  title: {
    fontSize: 12,
    fontWeight: '600',
    color: '#ffffff',
  },
});

export default Header;
