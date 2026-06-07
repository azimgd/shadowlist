import { memo } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import { colors, typography, radius } from '../theme';
import type { PollOption as PollOptionData } from './data';

export interface PollOptionProps {
  option: PollOptionData;
  total: number;
  leading?: boolean;
  onVote?: (id: string) => void;
}

// One option: icon chip, label, its share, and an inline result bar. The current
// leader is tinted with the app accent. Tapping anywhere on the row casts a vote.
export const PollOptionRow = memo(
  ({ option, total, leading = false, onVote }: PollOptionProps) => {
    const share = total > 0 ? option.votes / total : 0;
    const percent = Math.round(share * 100);
    return (
      <Pressable
        style={({ pressed }) => [styles.row, pressed && styles.rowPressed]}
        onPress={() => onVote?.(option.id)}
      >
        <View style={[styles.iconChip, leading && styles.iconChipLeading]}>
          <option.Icon
            size={22}
            color={leading ? colors.accent : colors.label}
          />
        </View>
        <View style={styles.body}>
          <View style={styles.bodyTop}>
            <Text style={styles.label}>{option.label}</Text>
            <Text style={[styles.share, leading && styles.shareLeading]}>
              {percent}%
            </Text>
          </View>
          <View style={styles.barTrack}>
            <View
              style={[
                styles.barFill,
                leading && styles.barFillLeading,
                { width: `${percent}%` },
              ]}
            />
          </View>
        </View>
      </Pressable>
    );
  }
);

const styles = StyleSheet.create({
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: colors.background,
    paddingHorizontal: 16,
    paddingVertical: 14,
  },
  rowPressed: {
    backgroundColor: colors.elevated,
  },
  iconChip: {
    width: 44,
    height: 44,
    borderRadius: radius.pill,
    backgroundColor: colors.elevated,
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: 14,
  },
  iconChipLeading: {
    backgroundColor: colors.accentSoft,
  },
  body: {
    flex: 1,
  },
  bodyTop: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  label: {
    color: colors.label,
    ...typography.headline,
  },
  share: {
    color: colors.secondaryLabel,
    ...typography.subhead,
  },
  shareLeading: {
    color: colors.accent,
    fontWeight: '600',
  },
  barTrack: {
    height: 6,
    borderRadius: radius.pill,
    backgroundColor: colors.fill,
    overflow: 'hidden',
  },
  barFill: {
    height: '100%',
    borderRadius: radius.pill,
    backgroundColor: colors.secondaryLabel,
  },
  barFillLeading: {
    backgroundColor: colors.accent,
  },
});
