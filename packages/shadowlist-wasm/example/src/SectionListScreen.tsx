import { memo, useMemo, type CSSProperties } from 'react';
import { SectionList, type SectionListData } from 'shadowlist-wasm';
import { AVATAR_COLORS, generateContact } from './constants';
import type { ContactElement as ContactElementType } from './ContactElement';

type Section = SectionListData<ContactElementType, { title: string }>;

const ContactRow = memo(
  ({ item, color }: { item: ContactElementType; color: string }) => {
    const initials = `${item.firstName.charAt(0)}${item.lastName.charAt(0)}`;
    return (
      <div style={styles.row}>
        <div style={{ ...styles.avatar, background: color }}>
          <span style={styles.initials}>{initials}</span>
        </div>
        <div style={styles.rowText}>
          <span style={styles.name}>
            {item.firstName} {item.lastName}
          </span>
          <span style={styles.phone}>{item.phoneNumber}</span>
        </div>
      </div>
    );
  }
);

export const SectionListScreen = () => {
  // Group contacts by the first letter of their last (or first) name.
  const sections = useMemo<Section[]>(() => {
    const contacts = Array.from({ length: 120 }, (_, index) =>
      generateContact(index)
    );
    const buckets = new Map<string, ContactElementType[]>();
    for (const contact of contacts) {
      const letter = (contact.lastName || contact.firstName)
        .charAt(0)
        .toUpperCase();
      const bucket = buckets.get(letter) ?? [];
      bucket.push(contact);
      buckets.set(letter, bucket);
    }
    return [...buckets.entries()]
      .sort(([a], [b]) => a.localeCompare(b))
      .map(([title, data]) => ({ key: title, title, data }));
  }, []);

  return (
    <div style={styles.container}>
      <SectionList<ContactElementType, { title: string }>
        sections={sections}
        style={styles.list}
        keyExtractor={(item) => item.id}
        renderSectionHeader={({ section }) => (
          <div style={styles.sectionHeader}>{section.title}</div>
        )}
        renderItem={({ item, index }) => (
          <ContactRow item={item} color={AVATAR_COLORS[index % AVATAR_COLORS.length]} />
        )}
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
  sectionHeader: {
    background: '#111113',
    color: '#FF9500',
    fontSize: 14,
    fontWeight: 700,
    padding: '6px 16px',
    borderBottom: '1px solid #2F3336',
    boxSizing: 'border-box',
  },
  row: {
    display: 'flex',
    alignItems: 'center',
    padding: '12px 16px',
    background: '#000000',
    borderBottom: '1px solid #1C1C1E',
    boxSizing: 'border-box',
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
    color: '#FFFFFF',
    fontSize: 15,
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
};
