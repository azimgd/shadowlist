import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';

interface FooterListItemProps {
  text?: string;
}

export const FooterListItem = memo(
  ({ text = 'End of list' }: FooterListItemProps) => {
    return (
      <View style={styles.container}>
        <Text style={styles.text}>{text}</Text>
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    backgroundColor: '#000000',
    paddingHorizontal: 12,
    paddingVertical: 20,
    marginTop: 8,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#2F3336',
    alignItems: 'center',
  },
  text: {
    color: '#71767B',
    fontSize: 14,
  },
});
