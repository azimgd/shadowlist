import { memo, type CSSProperties } from 'react';
import type { ActivityData } from 'shadowlist-utils';

interface ActivityElementProps {
  element: ActivityData;
}

export const ActivityElement = memo(({ element }: ActivityElementProps) => {
  return (
    <div style={styles.activityElement}>
      <div style={{ ...styles.avatar, backgroundColor: element.accent }}>
        <span style={styles.avatarText}>{element.actor.charAt(0)}</span>
      </div>
      <div style={styles.content}>
        <span style={styles.title}>
          <span style={styles.actor}>{element.actor}</span>
          <span style={styles.action}> {element.action}</span>
        </span>
        <span style={styles.detail}>{element.detail}</span>
      </div>
      <span style={styles.timestamp}>{element.timestamp}</span>
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  activityElement: {
    display: 'flex',
    flexDirection: 'row',
    alignItems: 'flex-start',
    background: '#000000',
    padding: '12px 12px',
  },
  avatar: {
    width: 40,
    height: 40,
    borderRadius: 20,
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
    flexShrink: 0,
  },
  avatarText: {
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: 700,
  },
  content: {
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minWidth: 0,
  },
  title: {
    fontSize: 15,
    marginBottom: 2,
    overflow: 'hidden',
    textOverflow: 'ellipsis',
    whiteSpace: 'nowrap',
  },
  actor: {
    color: '#FFFFFF',
    fontWeight: 700,
  },
  action: {
    color: '#71767B',
  },
  detail: {
    color: '#71767B',
    fontSize: 14,
    lineHeight: '18px',
    display: '-webkit-box',
    WebkitLineClamp: 2,
    WebkitBoxOrient: 'vertical',
    overflow: 'hidden',
  },
  timestamp: {
    color: '#71767B',
    fontSize: 13,
    marginLeft: 8,
    flexShrink: 0,
  },
};
