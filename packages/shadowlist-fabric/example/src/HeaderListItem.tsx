import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';

interface HeaderListItemProps {
  title?: string;
  subtitle?: string;
}

export const HeaderListItem = memo(
  ({ title = 'Header', subtitle }: HeaderListItemProps) => {
    return (
      <View style={styles.container}>
        <Text style={styles.title}>{title}</Text>
        {subtitle && <Text style={styles.subtitle}>{subtitle}</Text>}
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    backgroundColor: '#000000',
    padding: 12,
    marginBottom: 12,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#2F3336',
  },
  title: {
    color: '#FFFFFF',
    fontSize: 24,
    fontWeight: '700',
  },
  subtitle: {
    color: '#71767B',
    fontSize: 13,
    marginTop: 4,
  },
});
