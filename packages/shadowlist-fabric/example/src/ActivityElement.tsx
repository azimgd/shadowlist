import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import type { ActivityData } from 'shadowlist-utils';

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
    backgroundColor: '#000000',
    paddingHorizontal: 12,
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
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: '700',
  },
  content: {
    flex: 1,
  },
  title: {
    fontSize: 15,
    marginBottom: 2,
  },
  actor: {
    color: '#FFFFFF',
    fontWeight: '700',
  },
  action: {
    color: '#71767B',
  },
  detail: {
    color: '#71767B',
    fontSize: 14,
    lineHeight: 18,
  },
  timestamp: {
    color: '#71767B',
    fontSize: 13,
    marginLeft: 8,
  },
});
