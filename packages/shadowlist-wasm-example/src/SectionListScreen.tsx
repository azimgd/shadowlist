import { useMemo, type CSSProperties } from 'react';
import { type SectionListData } from 'shadowlist-wasm';
import { SectionList, ListHeader, colors } from 'shadowlist-utils/web';
import { generateContact, type ContactItem } from 'shadowlist-utils';

type Section = SectionListData<ContactItem, { title: string }>;

// Group contacts by the first letter of their last (or first) name.
const buildSections = (): Section[] => {
  const contacts = Array.from({ length: 120 }, (_, index) => generateContact(index));
  const buckets = new Map<string, ContactItem[]>();
  for (const contact of contacts) {
    const letter = (contact.lastName || contact.firstName).charAt(0).toUpperCase();
    const bucket = buckets.get(letter) ?? [];
    bucket.push(contact);
    buckets.set(letter, bucket);
  }
  return [...buckets.entries()]
    .sort(([a], [b]) => a.localeCompare(b))
    .map(([title, data]) => ({ key: title, title, data }));
};

export const SectionListScreen = () => {
  const sections = useMemo<Section[]>(buildSections, []);

  return (
    <div style={styles.container}>
      <SectionList.List
        sections={sections}
        style={styles.list}
        keyExtractor={(item) => item.id}
        renderItem={({ item, index }) => (
          <SectionList.Row element={item} index={index} />
        )}
        renderSectionHeader={({ section }) => (
          <SectionList.SectionHeader
            title={section.title}
            count={section.data.length}
          />
        )}
        ListHeaderComponent={
          <ListHeader title="Contacts" subtitle="Grouped, sticky sections" />
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
};
