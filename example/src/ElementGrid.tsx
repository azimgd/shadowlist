import {
  Image,
  StyleSheet,
  Text,
  Pressable,
  type GestureResponderEvent,
  type ViewStyle,
  View,
  Dimensions,
} from 'react-native';

type ElementProps = {
  data: Array<any>;
  onPress: (index: number) => void;
  style?: ViewStyle;
};

const ElementGrid = (props: ElementProps) => {
  const handlePress = (event: GestureResponderEvent) => {
    const elementDataIndex = __NATIVE_getRegistryElementMapping(
      event.nativeEvent.target
    );
    props.onPress(elementDataIndex);
  };

  return (
    <Pressable style={[styles.container, props.style]} onPress={handlePress}>
      <Image source={{ uri: `{{image}}` }} style={styles.image} />

      <View style={styles.bottom}>
        <Text style={styles.title} ellipsizeMode="clip">{`{{title}}`}</Text>
        <Text style={styles.subtitle}>{`{{subtitle}}`}</Text>
      </View>
    </Pressable>
  );
};

const styles = StyleSheet.create({
  container: {
    paddingHorizontal: 16,
    paddingVertical: 4,
  },
  bottom: {
    flex: 1,
    gap: 8,
  },
  title: {
    fontWeight: '600',
    color: '#ffffff',
  },
  subtitle: {
    fontWeight: '300',
    color: '#ffffff',
  },
  image: {
    borderRadius: 4,
    marginBottom: 4,
    width: Dimensions.get('window').width / 3 - 32,
    height: Dimensions.get('window').width / 3 - 32,
    backgroundColor: '#dddddd20',
  },
});

export default ElementGrid;
