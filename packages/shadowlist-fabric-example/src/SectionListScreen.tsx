import { useCallback, useMemo, useRef, useState } from 'react';
import { View, StyleSheet } from 'react-native';
import { type ShadowlistCommands, type SectionListData } from 'shadowlist';
import {
  SectionList,
  ListHeader,
  ListFooter,
  colors,
} from 'shadowlist-utils/native';
import { generateContact, type ContactItem } from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

type ContactSection = SectionListData<ContactItem, { title: string }>;

// Group contacts into A-Z sections by first-name initial, sorted within each.
const buildSections = (contacts: ContactItem[]): ContactSection[] => {
  const groups = new Map<string, ContactItem[]>();

  for (const contact of contacts) {
    const letter = (contact.firstName.charAt(0) || '#').toUpperCase();
    const group = groups.get(letter);
    if (group) group.push(contact);
    else groups.set(letter, [contact]);
  }

  return Array.from(groups.keys())
    .sort()
    .map((letter) => ({
      key: letter,
      title: letter,
      data: groups
        .get(letter)!
        .slice()
        .sort((a, b) =>
          `${a.firstName} ${a.lastName}`.localeCompare(
            `${b.firstName} ${b.lastName}`
          )
        ),
    }));
};

export const SectionListScreen = () => {
  const sectionListRef = useRef<ShadowlistCommands>(null);
  const [contacts, setContacts] = useState<ContactItem[]>(() =>
    Array.from({ length: 300 }, (_, index) => generateContact(index))
  );

  const sections = useMemo(() => buildSections(contacts), [contacts]);

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

  const handleDelete = useCallback((id: string) => {
    setContacts((prev) => prev.filter((contact) => contact.id !== id));
  }, []);

  const renderElement = useCallback(
    ({ element, index }: { element: ContactItem; index: number }) => (
      <SectionList.Row
        element={element}
        index={index}
        onDelete={handleDelete}
      />
    ),
    [handleDelete]
  );

  const renderSectionHeader = useCallback(
    ({ section }: { section: ContactSection }) => (
      <SectionList.SectionHeader
        title={section.title}
        count={section.data.length}
      />
    ),
    []
  );

  return (
    <View style={styles.container}>
      <SectionList.List
        ref={sectionListRef}
        sections={sections}
        style={styles.list}
        renderElement={renderElement}
        renderSectionHeader={renderSectionHeader}
        ListHeaderComponent={
          <ListHeader title="Contacts" subtitle="Grouped, sticky sections" />
        }
        ListFooterComponent={
          <ListFooter text={`${contacts.length} contacts`} />
        }
      />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: colors.background,
  },
  list: {
    flex: 1,
    backgroundColor: colors.background,
  },
});
