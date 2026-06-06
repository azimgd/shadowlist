import { memo, useState, type CSSProperties } from 'react';
import { Shadowlist } from 'shadowlist-wasm';
import { HeaderListItem } from './HeaderListItem';
import { AVATAR_COLORS, generateContact } from './constants';
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
        <span style={styles.grip}>≡</span>
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
    background: '#000000',
  },
  list: {
    flex: 1,
    minHeight: 0,
    background: '#000000',
  },
  row: {
    display: 'flex',
    alignItems: 'center',
    padding: '12px 16px',
    background: '#000000',
    borderBottom: '1px solid #1C1C1E',
    boxSizing: 'border-box',
    cursor: 'grab',
    userSelect: 'none',
  },
  avatar: {
    width: 44,
    height: 44,
    borderRadius: 22,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: 12,
    flexShrink: 0,
  },
  initials: {
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: 600,
  },
  rowText: {
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
  },
  name: {
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: 500,
  },
  phone: {
    color: '#8E8E93',
    fontSize: 13,
    marginTop: 2,
  },
  grip: {
    color: '#48484A',
    fontSize: 22,
    padding: '0 8px',
  },
};
