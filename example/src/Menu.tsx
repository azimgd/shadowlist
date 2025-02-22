import { View, StyleSheet, Text } from 'react-native';
import { MenuView } from '@react-native-menu/menu';

export type VariantsKey =
  | 'vertical-default'
  | 'vertical-inverted'
  | 'horizontal-default'
  | 'horizontal-inverted';

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
            titleColor: '#2367A2',
          },
          {
            id: 'vertical-inverted',
            title: 'Vertical Inverted',
            titleColor: '#2367A2',
          },
          {
            id: 'horizontal-default',
            title: 'Horizontal Default',
            titleColor: '#2367A2',
          },
          {
            id: 'horizontal-inverted',
            title: 'Horizontal Inverted',
            titleColor: '#2367A2',
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
    right: 0,
    bottom: 0,
    width: 100,
    height: 100,
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
