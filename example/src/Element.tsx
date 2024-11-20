import { StyleSheet, Text, View } from 'react-native';
import { type ShadowlistProps } from 'shadowlist';

const Element: ShadowlistProps['renderItem'] = ({ item, index }) => {
  return (
    <View style={styles.container}>
      <Text style={styles.title}>{item.id}</Text>
      <Text style={styles.content}>{item.text}</Text>
      <Text style={styles.footer}>index: {index}</Text>
    </View>
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
});

export default Element;
