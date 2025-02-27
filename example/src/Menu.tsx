import { View, StyleSheet, Text } from 'react-native';
import { MenuView } from '@react-native-menu/menu';

export type VariantsKey =
  | 'vertical-default'
  | 'vertical-inverted'
  | 'horizontal-default'
  | 'horizontal-inverted'
  | 'grid-masonry-3'
  | 'grid-aligned-3'
  | 'chatui'
  | 'initial-scroll-index'
  | 'scroll-to-index';

export type MenuProps = {
  onPress: (optionsKey: VariantsKey) => void;
};

export default function Menu(props: MenuProps) {
  return (
    <View style={styles.container}>
      <MenuView
        title="Menu Title"
        onPressAction={({ nativeEvent }) => {
          props.onPress(nativeEvent.event as VariantsKey);
        }}
        actions={[
          {
            id: 'vertical-default',
            title: 'Vertical Default',
          },
          {
            id: 'vertical-inverted',
            title: 'Vertical Inverted',
          },
          {
            id: 'horizontal-default',
            title: 'Horizontal Default',
          },
          {
            id: 'horizontal-inverted',
            title: 'Horizontal Inverted',
          },
          {
            id: 'grid-masonry-3',
            title: 'Grid Masonry Three',
          },
          {
            id: 'grid-aligned-3',
            title: 'Grid Aligned Three',
          },
          {
            id: 'chatui',
            title: 'Chat UI',
          },
          {
            id: 'initial-scroll-index',
            title: 'Initial Scroll Index Ten',
          },
          {
            id: 'scroll-to-index',
            title: 'Scroll to Index Ten',
          },
        ].toReversed()}
        shouldOpenOnLongPress={false}
      >
        <View style={styles.button}>
          <Text style={styles.icon}>🦾</Text>
        </View>
      </MenuView>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    position: 'absolute',
    right: 30,
    bottom: 30,
  },
  button: {
    width: 60,
    height: 60,
    borderRadius: 30,
    backgroundColor: '#1dd1a1',
    justifyContent: 'center',
    alignItems: 'center',
  },
  icon: {
    fontSize: 24,
  },
});
