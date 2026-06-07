import { memo, type CSSProperties } from 'react';
import { AVATAR_COLORS, type ContactItem } from 'shadowlist-utils';
import { colors, typography, ROW_INSET } from '../theme';
import { Grip } from '../icons';

export interface ReorderRowProps {
  element: ContactItem;
}

// Stable color keyed by id so a row keeps its color when reordered.
const colorForId = (id: string) => {
  let hash = 0;
  for (let i = 0; i < id.length; i++) hash = (hash * 31 + id.charCodeAt(i)) | 0;
  return AVATAR_COLORS[Math.abs(hash) % AVATAR_COLORS.length];
};

export const ReorderRow = memo(({ element }: ReorderRowProps) => {
  const avatarColor = colorForId(element.id);
  const initials = `${element.firstName.charAt(0)}${element.lastName.charAt(0)}`;

  return (
    <div style={styles.row}>
      <div style={{ ...styles.avatar, background: avatarColor }}>
        <span style={styles.initials}>{initials}</span>
      </div>
      <div style={styles.rowText}>
        <span style={styles.name}>
          {element.firstName} {element.lastName}
        </span>
        <span style={styles.phone}>{element.phoneNumber}</span>
      </div>
      <Grip size={20} color={colors.tertiaryLabel} />
      <div style={styles.separator} />
    </div>
  );
});

const styles: Record<string, CSSProperties> = {
  row: {
    position: 'relative',
    display: 'flex',
    alignItems: 'center',
    padding: '12px 16px',
    background: colors.background,
    boxSizing: 'border-box',
    cursor: 'grab',
    userSelect: 'none',
  },
  avatar: {
    width: 40,
    height: 40,
    borderRadius: 20,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: 12,
    flexShrink: 0,
  },
  initials: {
    color: colors.label,
    fontSize: 17,
    fontWeight: 600,
  },
  rowText: {
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minWidth: 0,
  },
  name: {
    color: colors.label,
    ...typography.body,
  },
  phone: {
    color: colors.secondaryLabel,
    ...typography.subhead,
    marginTop: 1,
  },
  separator: {
    position: 'absolute',
    left: ROW_INSET,
    right: 0,
    bottom: 0,
    height: 1,
    background: colors.separator,
  },
};
