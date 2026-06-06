import { memo, useState, type CSSProperties } from 'react';
import { Shadowlist } from 'shadowlist-wasm';
import { HeaderListItem } from './HeaderListItem';
import { AVATAR_COLORS, generateContact } from './constants';
import { colors, typography, ROW_INSET } from './theme';
import { Grip } from './icons';
import type { ContactElement as ContactElementType } from './ContactElement';

// A plain row; drag/reorder is handled by Shadowlist via the dragEnabled prop.
const ReorderElement = memo(
  ({ element, index }: { element: ContactElementType; index: number }) => {
    const avatarColor = AVATAR_COLORS[index % AVATAR_COLORS.length];
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
  }
);

export const ReorderScreen = () => {
  const [data, setData] = useState<ContactElementType[]>(() =>
    Array.from({ length: 80 }, (_, index) => generateContact(index))
  );

  return (
    <div style={styles.container}>
      <Shadowlist
        data={data}
        style={styles.list}
        dragEnabled
        onReorder={({ data: reordered }) => setData(reordered)}
        renderElement={({ element, index }) => (
          <ReorderElement element={element} index={index} />
        )}
        ListHeaderComponent={
          <HeaderListItem
            title="Reorder"
            subtitle="Press and hold a row, then drag to reorder"
          />
        }
      />
    </div>
  );
};

const styles: Record<string, CSSProperties> = {
  container: {
    position: 'relative',
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
  list: {
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
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
