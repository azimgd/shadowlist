import { memo } from 'react';
import {
  View,
  Text,
  Pressable,
  StyleSheet,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { formatRelativeTime } from '../formatRelativeTime';
import { useLabels } from '../labels';
import { Avatar } from '../primitives/Avatar';
import { useLargeText } from '../internal/useLargeText';
import { createStyles } from '../theme';
import { defaultActivityLabels, type ActivityLabels } from './labels';
import type { ActivityItem } from './types';

export interface ActivityRowProps {
  item: ActivityItem;
  onPress?: (item: ActivityItem) => void;
  formatTime?: (createdAt: Date | number) => string;
  labels?: Partial<ActivityLabels>;
  style?: StyleProp<ViewStyle>;
  avatarStyle?: StyleProp<ViewStyle>;
}

const AVATAR_SIZE = 40;

export const ActivityRow = memo(
  ({
    item,
    onPress,
    formatTime,
    labels,
    style,
    avatarStyle,
  }: ActivityRowProps) => {
    const styles = useStyles();
    const largeText = useLargeText();
    const l = useLabels(defaultActivityLabels, labels);
    const { actor } = item;
    const time = formatTime
      ? formatTime(item.createdAt)
      : formatRelativeTime(item.createdAt, l);
    const isUnread = item.read === false;
    const accessibilityLabel = [
      isUnread ? l.unread : undefined,
      `${actor.name} ${item.action}`,
      item.text,
      time,
    ]
      .filter(Boolean)
      .join(', ');

    return (
      <Pressable
        style={({ pressed }) => [
          styles.row,
          pressed && onPress !== undefined && styles.rowPressed,
          style,
        ]}
        disabled={onPress === undefined}
        onPress={onPress && (() => onPress(item))}
        accessible
        accessibilityRole={onPress ? 'button' : undefined}
        accessibilityLabel={accessibilityLabel}
      >
        {isUnread ? <View style={styles.unreadDot} /> : null}
        <Avatar
          name={actor.name}
          uri={actor.avatarUrl}
          color={actor.avatarColor}
          size={AVATAR_SIZE}
          style={[styles.avatar, avatarStyle]}
        />
        <View style={styles.content}>
          <Text style={styles.title} numberOfLines={largeText ? undefined : 1}>
            <Text style={styles.actor}>{actor.name}</Text>
            <Text style={styles.action}> {item.action}</Text>
          </Text>
          {item.text ? (
            <Text style={styles.text} numberOfLines={largeText ? 4 : 2}>
              {item.text}
            </Text>
          ) : null}
        </View>
        <Text style={styles.timestamp}>{time}</Text>
      </Pressable>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    row: {
      backgroundColor: theme.colors.background,
      paddingHorizontal: theme.spacing.lg,
      paddingVertical: theme.spacing.md,
      flexDirection: 'row',
      alignItems: 'flex-start',
    },
    rowPressed: {
      backgroundColor: theme.colors.elevated,
    },
    unreadDot: {
      position: 'absolute',
      left: theme.spacing.xs,
      top: theme.spacing.md + (AVATAR_SIZE - theme.spacing.sm) / 2,
      width: theme.spacing.sm,
      height: theme.spacing.sm,
      borderRadius: theme.spacing.sm / 2,
      backgroundColor: theme.colors.accent,
    },
    avatar: {
      marginRight: theme.spacing.md,
    },
    content: {
      flex: 1,
    },
    title: {
      ...theme.typography.subhead,
      marginBottom: theme.spacing.xxs,
    },
    actor: {
      color: theme.colors.label,
      fontWeight: theme.fontWeight.semibold,
    },
    action: {
      color: theme.colors.secondaryLabel,
    },
    text: {
      color: theme.colors.secondaryLabel,
      ...theme.typography.footnote,
    },
    timestamp: {
      color: theme.colors.tertiaryLabel,
      ...theme.typography.footnote,
      marginLeft: theme.spacing.sm,
    },
  })
);
