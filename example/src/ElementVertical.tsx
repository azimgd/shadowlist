import { useEffect, useState } from 'react';
import {
  Image,
  StyleSheet,
  Text,
  Pressable,
  type GestureResponderEvent,
  type ViewStyle,
  View,
} from 'react-native';

type ElementVerticalProps = {
  data: Array<any>;
  onPress: (index: number) => void;
  style?: ViewStyle;
};

const ElementVertical = (props: ElementVerticalProps) => {
  const handlePress = (event: GestureResponderEvent) => {
    const elementDataIndex = __NATIVE_getRegistryElementMapping(
      event.nativeEvent.target
    );
    props.onPress(elementDataIndex);
  };

  return (
    <Pressable style={[styles.container, props.style]} onPress={handlePress}>
      <View style={styles.left}>
        <Image source={{ uri: `{{image}}` }} style={styles.image} />
        <View style={styles.indicator} />
      </View>

      <View style={styles.right}>
        <Text style={styles.title}>{`{{title}}`}</Text>
        <Text style={styles.subtitle}>{`{{subtitle}}`}</Text>
        <Text style={styles.content}>{`{{text}}`}</Text>
      </View>
    </Pressable>
  );
};

type ElementVerticalDynamicProps = {
  item: any;
  style?: ViewStyle;
};

export const ElementVerticalDynamic = (props: ElementVerticalDynamicProps) => {
  const [timer, setTimer] = useState('');
  const [color, setColor] = useState('#ffffff');

  useEffect(() => {
    const interval = setInterval(() => {
      const randomColor = Math.floor(Math.random() * 16777215).toString(16);
      setTimer(Math.random().toFixed(2));
      setColor(`#${randomColor}`);
    }, 1000);
    return () => clearInterval(interval);
  }, []);

  return (
    <Pressable style={[styles.container, props.style]} onPress={() => {}}>
      <View style={styles.left}>
        <Image source={{ uri: props.item?.image }} style={styles.image} />
        <View style={[styles.indicator, { backgroundColor: color }]} />
      </View>

      <View style={styles.right}>
        <Text style={styles.title}>{props.item?.title + timer}</Text>
        <Text style={styles.subtitle}>{props.item?.subtitle}</Text>
        <Text style={styles.content}>{props.item?.text}</Text>
      </View>
    </Pressable>
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
  left: {
    alignItems: 'center',
    gap: 16,
  },
  right: {
    flex: 1,
  },
  title: {
    fontWeight: '600',
    color: '#ffffff',
    marginBottom: 4,
  },
  content: {
    fontWeight: '500',
    color: '#ffffff',
  },
  subtitle: {
    fontWeight: '300',
    color: '#ffffff',
    marginBottom: 16,
  },
  image: {
    borderRadius: 4,
    width: 60,
    height: 60,
    backgroundColor: '#dddddd20',
  },
  indicator: {
    width: 8,
    height: 8,
    borderRadius: 8,
    backgroundColor: '#1dd1a1',
  },
});

export default ElementVertical;
