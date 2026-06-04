import { memo } from 'react';
import { View, Text, TouchableOpacity, StyleSheet } from 'react-native';

interface ActivityHeaderProps {
  startThreshold: number;
  endThreshold: number;
  onScrollToOffset: () => void;
  onScrollToEnd: () => void;
  onScrollToRandom: () => void;
  onCycleStartThreshold: () => void;
  onCycleEndThreshold: () => void;
}

export const ActivityHeader = memo(
  ({
    startThreshold,
    endThreshold,
    onScrollToOffset,
    onScrollToEnd,
    onScrollToRandom,
    onCycleStartThreshold,
    onCycleEndThreshold,
  }: ActivityHeaderProps) => {
    return (
      <View style={styles.container}>
        <Text style={styles.title}>Activity</Text>
        <Text style={styles.subtitle}>Imperative scroll & reach thresholds</Text>

        <View style={styles.row}>
          <TouchableOpacity style={styles.action} onPress={onScrollToOffset}>
            <Text style={styles.actionText}>Offset 2000</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.action} onPress={onScrollToEnd}>
            <Text style={styles.actionText}>Scroll to end</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.action} onPress={onScrollToRandom}>
            <Text style={styles.actionText}>Random</Text>
          </TouchableOpacity>
        </View>

        <View style={styles.row}>
          <TouchableOpacity style={styles.chip} onPress={onCycleStartThreshold}>
            <Text style={styles.chipLabel}>Start threshold</Text>
            <Text style={styles.chipValue}>{startThreshold}</Text>
          </TouchableOpacity>
          <TouchableOpacity style={styles.chip} onPress={onCycleEndThreshold}>
            <Text style={styles.chipLabel}>End threshold</Text>
            <Text style={styles.chipValue}>{endThreshold}</Text>
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
  chip: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    backgroundColor: '#1C1C1E',
    borderRadius: 16,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: '#2F3336',
    paddingHorizontal: 12,
    paddingVertical: 7,
  },
  chipLabel: {
    color: '#71767B',
    fontSize: 13,
  },
  chipValue: {
    color: '#FF9500',
    fontSize: 13,
    fontWeight: '700',
  },
});
