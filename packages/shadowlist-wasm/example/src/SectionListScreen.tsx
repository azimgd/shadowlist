import { memo, useMemo, type CSSProperties } from 'react';
import { SectionList, type SectionListData } from 'shadowlist-wasm';
import { AVATAR_COLORS, generateContact } from './constants';
import { colors, typography, ROW_INSET } from './theme';
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
        <div style={styles.separator} />
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
    background: colors.background,
  },
  list: {
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
  sectionHeader: {
    background: colors.elevated,
    color: colors.secondaryLabel,
    ...typography.footnote,
    fontWeight: 600,
    textTransform: 'uppercase',
    padding: '10px 16px',
    boxSizing: 'border-box',
  },
  row: {
    position: 'relative',
    display: 'flex',
    alignItems: 'center',
    padding: '12px 16px',
    background: colors.background,
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
