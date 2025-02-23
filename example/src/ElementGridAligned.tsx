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

const ElementGridAligned = (props: ElementProps) => {
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
        <Text
          style={styles.title}
          ellipsizeMode="clip"
          numberOfLines={1}
        >{`{{title}}`}</Text>
      </View>
    </Pressable>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  bottom: {
    position: 'absolute',
    bottom: 0,
    left: 0,
    right: 0,
    gap: 8,
    backgroundColor: '#33333380',
  },
  title: {
    paddingHorizontal: 16,
    paddingVertical: 4,
    fontWeight: '600',
    color: '#ffffff',
  },
  subtitle: {
    fontWeight: '300',
    color: '#ffffff',
  },
  image: {
    width: Dimensions.get('window').width / 3,
    height: Dimensions.get('window').width / 3,
    backgroundColor: '#dddddd20',
  },
});

export default ElementGridAligned;
