import { memo, type CSSProperties } from 'react';
import type { ActivityData } from 'shadowlist-utils';
import { colors, typography } from './theme';

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
    background: colors.background,
    padding: '12px 16px',
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
    color: colors.label,
    fontSize: 17,
    fontWeight: 600,
  },
  content: {
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minWidth: 0,
  },
  title: {
    ...typography.subhead,
    marginBottom: 2,
    overflow: 'hidden',
    textOverflow: 'ellipsis',
    whiteSpace: 'nowrap',
  },
  actor: {
    color: colors.label,
    fontWeight: 600,
  },
  action: {
    color: colors.secondaryLabel,
  },
  detail: {
    color: colors.secondaryLabel,
    ...typography.footnote,
    display: '-webkit-box',
    WebkitLineClamp: 2,
    WebkitBoxOrient: 'vertical',
    overflow: 'hidden',
  },
  timestamp: {
    color: colors.tertiaryLabel,
    ...typography.footnote,
    marginLeft: 8,
    flexShrink: 0,
  },
};
