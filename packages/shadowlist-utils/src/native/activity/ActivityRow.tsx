import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import type { ActivityData } from 'shadowlist-utils';
import {
  colors,
  typography,
  spacing,
  radius,
  fontSize,
  fontWeight,
} from '../theme';

export interface ActivityRowProps {
  element: ActivityData;
}

export const ActivityRow = memo(({ element }: ActivityRowProps) => {
  return (
    <View style={styles.activityElement}>
      <View style={[styles.avatar, { backgroundColor: element.accent }]}>
        <Text style={styles.avatarText}>{element.actor.charAt(0)}</Text>
      </View>
      <View style={styles.content}>
        <Text style={styles.title} numberOfLines={1}>
          <Text style={styles.actor}>{element.actor}</Text>
          <Text style={styles.action}> {element.action}</Text>
        </Text>
        <Text style={styles.detail} numberOfLines={2}>
          {element.detail}
        </Text>
      </View>
      <Text style={styles.timestamp}>{element.timestamp}</Text>
    </View>
  );
});

const styles = StyleSheet.create({
  activityElement: {
    backgroundColor: colors.background,
    paddingLeft: spacing.lg,
    paddingRight: spacing.lg,
    paddingVertical: spacing.md,
    flexDirection: 'row',
    alignItems: 'flex-start',
  },
  avatar: {
    width: 40,
    height: 40,
    borderRadius: radius.xl,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: spacing.md,
  },
  avatarText: {
    color: colors.label,
    fontSize: fontSize.body,
    fontWeight: fontWeight.semibold,
  },
  content: {
    flex: 1,
  },
  title: {
    ...typography.subhead,
    marginBottom: spacing.xxs,
  },
  actor: {
    color: colors.label,
    fontWeight: fontWeight.semibold,
  },
  action: {
    color: colors.secondaryLabel,
  },
  detail: {
    color: colors.secondaryLabel,
    ...typography.footnote,
  },
  timestamp: {
    color: colors.tertiaryLabel,
    ...typography.footnote,
    marginLeft: spacing.sm,
  },
});
