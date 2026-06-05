import { useMemo, useRef, useState } from 'react';
import { View, StyleSheet } from 'react-native';
import {
  SectionList,
  type ShadowlistCommands,
  type SectionListData,
} from 'shadowlist';
import { FloatingActionBar } from './FloatingActionBar';
import {
  ContactElement,
  type ContactElement as ContactElementType,
} from './ContactElement';
import { SectionHeaderListItem } from './SectionHeaderListItem';
import { HeaderListItem } from './HeaderListItem';
import { FooterListItem } from './FooterListItem';
import { ListItemSeparator } from './ListItemSeparator';
import { generateContact } from './constants';

type ContactSection = SectionListData<ContactElementType, { title: string }>;

/*
 * Group a flat list of contacts into alphabetical sections by the first letter of
 * the first name, sorted A-Z, with contacts sorted within each section. Mirrors the
 * classic phone-book SectionList the native sticky section headers are built for.
 */
const buildSections = (contacts: ContactElementType[]): ContactSection[] => {
  const groups = new Map<string, ContactElementType[]>();

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
  const [contacts, setContacts] = useState<ContactElementType[]>(() =>
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

  const handleScrollToIndex = (index: number) => {
    sectionListRef.current?.scrollToIndex(index);
  };

  return (
    <View style={styles.container}>
      <SectionList
        ref={sectionListRef}
        sections={sections}
        style={styles.list}
        renderItem={({ item, index }) => (
          <ContactElement element={item} index={index} />
        )}
        renderSectionHeader={({ section }) => (
          <SectionHeaderListItem
            title={section.title}
            count={section.data.length}
          />
        )}
        ItemSeparatorComponent={<ListItemSeparator />}
        ListHeaderComponent={
          <HeaderListItem
            title="Contacts"
            subtitle="Grouped, sticky sections"
          />
        }
        ListFooterComponent={
          <FooterListItem text={`${contacts.length} contacts`} />
        }
      />
      <FloatingActionBar
        onPrepend={handlePrepend}
        onAppend={handleAppend}
        onScrollToIndex={handleScrollToIndex}
        dataLength={contacts.length}
      />
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000000',
  },
  list: {
    flex: 1,
    backgroundColor: '#000000',
  },
});
