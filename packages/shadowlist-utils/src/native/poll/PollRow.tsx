import { memo } from 'react';
import {
  View,
  Text,
  Pressable,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { defaultPollLabels, type PollLabels } from './labels';
import type { PollOption } from './types';

export interface PollRowProps {
  item: PollOption;
  totalVotes: number;
  isLeading?: boolean;
  isSelected?: boolean;
  onVote?: (optionId: string) => void;
  labels?: Partial<PollLabels>;
  style?: StyleProp<ViewStyle>;
}

export const PollRow = memo(
  ({
    item: option,
    totalVotes,
    isLeading = false,
    isSelected = false,
    onVote,
    labels,
    style,
  }: PollRowProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const l = useLabels(defaultPollLabels, labels);
    const percent =
      totalVotes > 0 ? Math.round((option.votes / totalVotes) * 100) : 0;
    const Icon = option.icon;
    return (
      <Pressable
        style={({ pressed }) => [
          styles.row,
          pressed && styles.rowPressed,
          style,
        ]}
        onPress={onVote && (() => onVote(option.id))}
        accessibilityRole="radio"
        accessibilityState={{ selected: isSelected, checked: isSelected }}
        accessibilityLabel={l.option(option.label, percent)}
      >
        <View
          style={[
            styles.iconChip,
            isLeading && styles.iconChipLeading,
            isSelected && styles.iconChipSelected,
          ]}
        >
          {Icon ? (
            <Icon
              size={22}
              color={isLeading ? theme.colors.accent : theme.colors.label}
            />
          ) : (
            <Text style={[styles.initial, isLeading && styles.accentText]}>
              {Array.from(option.label)[0]?.toUpperCase()}
            </Text>
          )}
        </View>
        <View style={styles.body}>
          <View style={styles.bodyTop}>
            <Text style={styles.label}>{option.label}</Text>
            <Text style={[styles.share, isLeading && styles.shareLeading]}>
              {percent}%
            </Text>
          </View>
          <View style={styles.barTrack}>
            <View
              style={[
                styles.barFill,
                isLeading && styles.barFillLeading,
                { width: `${percent}%` },
              ]}
            />
          </View>
        </View>
      </Pressable>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    row: {
      flexDirection: 'row',
      alignItems: 'center',
      backgroundColor: theme.colors.background,
      paddingHorizontal: theme.spacing.lg,
      paddingVertical: 14,
    },
    rowPressed: {
      backgroundColor: theme.colors.elevated,
    },
    iconChip: {
      width: 44,
      height: 44,
      borderRadius: theme.radius.pill,
      backgroundColor: theme.colors.elevated,
      alignItems: 'center',
      justifyContent: 'center',
      marginRight: 14,
    },
    iconChipLeading: {
      backgroundColor: theme.colors.accentSoft,
    },
    iconChipSelected: {
      borderWidth: 2,
      borderColor: theme.colors.accent,
    },
    initial: {
      color: theme.colors.label,
      ...theme.typography.headline,
    },
    accentText: {
      color: theme.colors.accent,
    },
    body: {
      flex: 1,
    },
    bodyTop: {
      flexDirection: 'row',
      justifyContent: 'space-between',
      alignItems: 'center',
      marginBottom: theme.spacing.sm,
    },
    label: {
      color: theme.colors.label,
      ...theme.typography.headline,
    },
    share: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.subhead,
    },
    shareLeading: {
      color: theme.colors.accent,
      fontWeight: theme.fontWeight.semibold,
    },
    barTrack: {
      height: 6,
      borderRadius: theme.radius.pill,
      backgroundColor: theme.colors.fill,
      overflow: 'hidden',
    },
    barFill: {
      height: '100%',
      borderRadius: theme.radius.pill,
      backgroundColor: theme.colors.secondaryLabel,
    },
    barFillLeading: {
      backgroundColor: theme.colors.accent,
    },
  })
);
