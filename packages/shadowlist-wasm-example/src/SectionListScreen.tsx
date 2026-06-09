import { useMemo, useRef, useState, type CSSProperties } from 'react';
import { type ShadowlistCommands, type SectionListData } from 'shadowlist-wasm';
import {
  SectionList,
  ListHeader,
  ListFooter,
  colors,
} from 'shadowlist-utils/web';
import {
  generateContact,
  groupContactsByInitial,
  type ContactItem,
} from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

type ContactSection = SectionListData<ContactItem, { title: string }>;

export const SectionListScreen = () => {
  const sectionListRef = useRef<ShadowlistCommands>(null);
  const [contacts, setContacts] = useState<ContactItem[]>(() =>
    Array.from({ length: 300 }, (_, index) => generateContact(index))
  );

  const sections = useMemo<ContactSection[]>(
    () => groupContactsByInitial(contacts),
    [contacts]
  );

  const handlePrepend = () => {
    const currentLength = contacts.length;
    const newContacts = Array.from({ length: 10 }, (_, index) =>
      generateContact(currentLength + index)
    );
    setContacts((prev) => [...newContacts, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = contacts.length;
    const newContacts = Array.from({ length: 10 }, (_, index) =>
      generateContact(currentLength + index)
    );
    setContacts((prev) => [...prev, ...newContacts]);
  };

  const handleScrollToRandom = () => {
    sectionListRef.current?.scrollToIndex(
      Math.floor(Math.random() * contacts.length)
    );
  };

  useHeaderActions({
    onPrepend: handlePrepend,
    onAppend: handleAppend,
    onScrollToRandom: handleScrollToRandom,
  });

  const handleDelete = (id: string) => {
    setContacts((prev) => prev.filter((contact) => contact.id !== id));
  };

  return (
    <div style={styles.container}>
      <SectionList.List
        ref={sectionListRef}
        sections={sections}
        style={styles.list}
        renderItem={({ item, index }) => (
          <SectionList.Row
            element={item}
            index={index}
            onDelete={handleDelete}
          />
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
        ListFooterComponent={
          <ListFooter text={`${contacts.length} contacts`} />
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
