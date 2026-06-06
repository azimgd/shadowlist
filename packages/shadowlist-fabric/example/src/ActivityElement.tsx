import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import type { ActivityData } from 'shadowlist-utils';
import { colors, typography } from './theme';

interface ActivityElementProps {
  element: ActivityData;
}

export const ActivityElement = memo(({ element }: ActivityElementProps) => {
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
    paddingLeft: 16,
    paddingRight: 16,
    paddingVertical: 12,
    flexDirection: 'row',
    alignItems: 'flex-start',
  },
  avatar: {
    width: 40,
    height: 40,
    borderRadius: 20,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
  },
  avatarText: {
    color: colors.label,
    fontSize: 17,
    fontWeight: '600',
  },
  content: {
    flex: 1,
  },
  title: {
    ...typography.subhead,
    marginBottom: 2,
  },
  actor: {
    color: colors.label,
    fontWeight: '600',
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
    marginLeft: 8,
  },
});
