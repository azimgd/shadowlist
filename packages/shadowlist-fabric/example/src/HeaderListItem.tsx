import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';

interface HeaderListItemProps {
  title?: string;
  subtitle?: string;
}

export const HeaderListItem = memo(({ title = 'Header', subtitle }: HeaderListItemProps) => {
  const insets = useSafeAreaInsets();

  return (
    <View style={[styles.container, { paddingTop: insets.top + 20 }]}>
      <Text style={styles.title}>{title}</Text>
      {subtitle && <Text style={styles.subtitle}>{subtitle}</Text>}
    </View>
  );
});

const styles = StyleSheet.create({
  container: {
    backgroundColor: '#000000',
    paddingHorizontal: 16,
    paddingBottom: 20,
    marginBottom: 8,
    borderBottomWidth: 0.5,
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
