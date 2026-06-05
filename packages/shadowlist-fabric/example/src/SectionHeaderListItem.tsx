import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';

interface SectionHeaderListItemProps {
  title: string;
  count?: number;
}

export const SectionHeaderListItem = memo(
  ({ title, count }: SectionHeaderListItemProps) => {
    return (
      <View style={styles.container}>
        <Text style={styles.title}>{title}</Text>
        {count !== undefined && <Text style={styles.count}>{count}</Text>}
      </View>
    );
  }
);

const styles = StyleSheet.create({
  // Opaque so the pinned header fully covers rows scrolling underneath it.
  container: {
    backgroundColor: '#15181C',
    paddingHorizontal: 12,
    paddingVertical: 6,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#2F3336',
  },
  title: {
    color: '#FF9500',
    fontSize: 15,
    fontWeight: '700',
  },
  count: {
    color: '#71767B',
    fontSize: 13,
    fontWeight: '500',
  },
});
