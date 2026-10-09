import { memo, useCallback } from 'react';
import {
  Pressable,
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import type { Separators } from 'shadowlist';
import { Avatar } from '../primitives/Avatar';
import { CheckIcon } from '../icons';
import { createStyles, useTheme } from '../theme';
import { formatRelativeTime } from '../formatRelativeTime';
import { defaultInboxLabels, type InboxLabels } from './labels';
import type { InboxMessage } from './types';

export interface InboxRowProps {
  item: InboxMessage;
  onPress?: (item: InboxMessage) => void;
  editing?: boolean;
  selected?: boolean;
  separators?: Separators;
  labels?: InboxLabels;
  style?: StyleProp<ViewStyle>;
}

const TIME_LABELS = {
  now: 'now',
  minutes: (count: number) => `${count}m`,
  hours: (count: number) => `${count}h`,
  days: (count: number) => `${count}d`,
};

/*
 * A mail row: avatar, sender, time, subject and a two line preview. An unread dot and a flag
 * mark its state. While editing it shows a selection circle. Pressing it highlights the
 * separators around it, like a native list row.
 */
export const InboxRow = memo(
  ({
    item,
    onPress,
    editing = false,
    selected = false,
    separators,
    labels = defaultInboxLabels,
    style,
  }: InboxRowProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const handlePress = useCallback(() => onPress?.(item), [onPress, item]);
    const states = [
      item.read ? null : labels.unreadState,
      item.flagged ? labels.flaggedState : null,
      editing && selected ? labels.selected : null,
    ].filter((state): state is string => state !== null);

    return (
      <Pressable
        onPress={onPress !== undefined ? handlePress : undefined}
        onPressIn={separators?.highlight}
        onPressOut={separators?.unhighlight}
        accessible
        accessibilityRole="button"
        accessibilityState={editing ? { selected } : undefined}
        accessibilityLabel={[
          ...states,
          item.sender,
          item.subject,
          item.preview,
        ].join(', ')}
        style={({ pressed }) => [
          styles.row,
          (pressed || (editing && selected)) && styles.highlighted,
          style,
        ]}
      >
        {editing ? (
          <View
            style={[styles.check, selected && styles.checkSelected]}
            testID={selected ? 'inbox-check-on' : 'inbox-check-off'}
          >
            {selected ? (
              <CheckIcon size={14} color={theme.colors.onAccent} />
            ) : null}
          </View>
        ) : (
          <View style={styles.gutter}>
            {item.read ? null : <View style={styles.unreadDot} />}
          </View>
        )}
        <Avatar name={item.sender} uri={item.avatarUrl} size={40} />
        <View style={styles.body}>
          <View style={styles.titleLine}>
            <Text
              style={[styles.sender, !item.read && styles.senderUnread]}
              numberOfLines={1}
            >
              {item.sender}
            </Text>
            {item.flagged ? <View style={styles.flag} /> : null}
            <Text style={styles.time}>
              {formatRelativeTime(item.receivedAt, TIME_LABELS)}
            </Text>
          </View>
          <Text style={styles.subject} numberOfLines={1}>
            {item.subject}
          </Text>
          <Text style={styles.preview} numberOfLines={2}>
            {item.preview}
          </Text>
        </View>
      </Pressable>
    );
  }
);

const useStyles = createStyles(({ colors, spacing, typography, fontWeight }) =>
  StyleSheet.create({
    row: {
      flexDirection: 'row',
      alignItems: 'flex-start',
      paddingRight: spacing.lg,
      paddingVertical: spacing.md,
      backgroundColor: colors.background,
    },
    highlighted: {
      backgroundColor: colors.fill,
    },
    gutter: {
      width: 28,
      alignItems: 'center',
      paddingTop: 16,
    },
    unreadDot: {
      width: 10,
      height: 10,
      borderRadius: 5,
      backgroundColor: colors.accent,
    },
    check: {
      width: 22,
      height: 22,
      borderRadius: 11,
      marginHorizontal: 3,
      marginTop: 9,
      borderWidth: 1.5,
      borderColor: colors.tertiaryLabel,
      alignItems: 'center',
      justifyContent: 'center',
    },
    checkSelected: {
      borderColor: colors.accent,
      backgroundColor: colors.accent,
    },
    body: {
      flex: 1,
      marginLeft: spacing.md,
    },
    titleLine: {
      flexDirection: 'row',
      alignItems: 'center',
    },
    sender: {
      flex: 1,
      ...typography.body,
      color: colors.label,
    },
    senderUnread: {
      fontWeight: fontWeight.semibold,
    },
    flag: {
      width: 8,
      height: 8,
      borderRadius: 2,
      marginHorizontal: spacing.xs,
      backgroundColor: colors.orange,
    },
    time: {
      ...typography.footnote,
      color: colors.secondaryLabel,
    },
    subject: {
      ...typography.subhead,
      color: colors.label,
    },
    preview: {
      ...typography.subhead,
      color: colors.secondaryLabel,
    },
  })
);
