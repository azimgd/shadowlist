import { memo } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import { colors, typography, radius } from './theme';

interface ActivityHeaderProps {
  onScrollToOffset: () => void;
  onScrollToEnd: () => void;
  onRemoveItems: () => void;
}

const TintedButton = ({
  label,
  onPress,
}: {
  label: string;
  onPress: () => void;
}) => (
  <Pressable
    onPress={onPress}
    style={({ pressed }) => [styles.action, pressed && styles.actionPressed]}
  >
    <Text style={styles.actionText}>{label}</Text>
  </Pressable>
);

export const ActivityHeader = memo(
  ({ onScrollToOffset, onScrollToEnd, onRemoveItems }: ActivityHeaderProps) => {
    return (
      <View style={styles.container}>
        <Text style={styles.title}>Activity</Text>
        <Text style={styles.subtitle}>
          Imperative scroll &amp; list editing, opens at index 30
        </Text>

        <View style={styles.row}>
          <TintedButton label="Offset 2000" onPress={onScrollToOffset} />
          <TintedButton label="Scroll to end" onPress={onScrollToEnd} />
          <TintedButton label="Remove 20 & 50" onPress={onRemoveItems} />
        </View>
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    backgroundColor: colors.background,
    paddingHorizontal: 16,
    paddingTop: 4,
    paddingBottom: 12,
  },
  title: {
    color: colors.label,
    ...typography.largeTitle,
  },
  subtitle: {
    color: colors.secondaryLabel,
    ...typography.subhead,
    marginTop: 2,
  },
  row: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 8,
    marginTop: 14,
  },
  action: {
    backgroundColor: colors.accentSoft,
    borderRadius: radius.sm,
    paddingHorizontal: 14,
    paddingVertical: 8,
  },
  actionPressed: {
    opacity: 0.6,
  },
  actionText: {
    color: colors.accent,
    ...typography.footnote,
    fontWeight: '600',
  },
});
