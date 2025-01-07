import {
  Image,
  StyleSheet,
  Text,
  TouchableOpacity,
  type GestureResponderEvent,
} from 'react-native';

const stringify = (str: string) => `{{${str}}}`;

type ElementProps = {
  data: Array<any>;
};

const Element = (props: ElementProps) => {
  const handlePress = (event: GestureResponderEvent) => {
    const elementDataIndex = __NATIVE_getRegistryElementMapping(
      event.nativeEvent.target
    );
    console.log(props.data[elementDataIndex]);
  };

  return (
    <TouchableOpacity style={styles.container} onPress={handlePress}>
      <Image source={{ uri: stringify('image') }} style={styles.image} />
      <Text style={styles.title}>{stringify('id')}</Text>
      <Text style={styles.content}>{stringify('text')}</Text>
      <Text style={styles.footer}>index: {stringify('position')}</Text>
    </TouchableOpacity>
  );
};

const styles = StyleSheet.create({
  container: {
    padding: 16,
    margin: 16,
    backgroundColor: '#eeeeee',
    borderRadius: 8,
  },
  title: {
    fontWeight: '600',
    color: '#333333',
    marginBottom: 16,
  },
  content: {
    fontWeight: '400',
    color: '#333333',
  },
  footer: {
    fontWeight: '400',
    color: '#333333',
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: '#33333330',
    marginTop: 8,
    paddingTop: 8,
  },
  image: {
    width: 100,
    height: 100,
  },
});

export default Element;
