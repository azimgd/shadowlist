import { memo } from 'react';
import { View, Text, TouchableOpacity, StyleSheet } from 'react-native';

interface ActivityHeaderProps {
  onScrollToOffset: () => void;
  onScrollToEnd: () => void;
  onRemoveItems: () => void;
}

export const ActivityHeader = memo(
  ({ onScrollToOffset, onScrollToEnd, onRemoveItems }: ActivityHeaderProps) => {
    return (
      <View style={styles.container}>
        <Text style={styles.title}>Activity</Text>
        <Text style={styles.subtitle}>Imperative scroll & list editing</Text>

        <View style={styles.row}>
          <TouchableOpacity style={styles.action} onPress={onScrollToOffset}>
            <Text style={styles.actionText}>Offset 2000</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.action} onPress={onScrollToEnd}>
            <Text style={styles.actionText}>Scroll to end</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.action} onPress={onRemoveItems}>
            <Text style={styles.actionText}>Remove 20 & 50</Text>
          </TouchableOpacity>
        </View>
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
  row: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 8,
    marginTop: 12,
  },
  action: {
    backgroundColor: '#FF9500',
    borderRadius: 16,
    paddingHorizontal: 12,
    paddingVertical: 7,
  },
  actionText: {
    color: '#FFFFFF',
    fontSize: 13,
    fontWeight: '600',
  },
});
