import { useMemo, useRef } from 'react';
import { View } from 'react-native';
import { type SectionListCommands } from 'shadowlist';
import { groupIntoSections } from 'shadowlist-utils';
import {
  Contacts,
  ListFooter,
  type ContactItem,
} from 'shadowlist-utils/native';
import { useScreenStyles } from './screenStyles';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { useContactActions } from './contactActions';
import { useAddContacts, useContactsQuery } from '../queries/contacts';

const NO_CONTACTS: ContactItem[] = [];

const initialOf = (contact: ContactItem) =>
  (contact.name.charAt(0) || '#').toUpperCase();

const byName = (a: ContactItem, b: ContactItem) => a.name.localeCompare(b.name);

export const SectionListScreen = () => {
  const styles = useScreenStyles();
  const sectionListRef = useRef<SectionListCommands>(null);

  const contacts = useContactsQuery();
  const { mutate: addContacts } = useAddContacts();
  const { openContact, removeContact } = useContactActions();

  const sections = useMemo(
    () =>
      groupIntoSections(contacts.data ?? NO_CONTACTS, {
        getSectionTitle: initialOf,
        compareItems: byName,
      }),
    [contacts.data]
  );

  const titles = useMemo(
    () => sections.map((section) => section.title),
    [sections]
  );

  useHeaderActions({
    onPrepend: () => addContacts({ count: 10, position: 'start' }),
    onAppend: () => addContacts({ count: 10, position: 'end' }),
    onScrollToRandom: () => {
      const sectionIndex = Math.floor(Math.random() * sections.length);
      const count = sections[sectionIndex]?.data.length ?? 0;
      // itemIndex 0 is the section header, 1 its first traveller.
      sectionListRef.current?.scrollToLocation({
        sectionIndex,
        itemIndex: 1 + Math.floor(Math.random() * count),
        animated: true,
      });
    },
    prependLabel: 'Add Travellers',
    appendLabel: 'Add More Travellers',
  });

  if (contacts.data === undefined) {
    return <QueryStatus error={contacts.error} onRetry={contacts.refetch} />;
  }

  return (
    <View style={styles.container}>
      <Contacts.SectionList
        ref={sectionListRef}
        sections={sections}
        style={styles.list}
        onPressItem={openContact}
        onDelete={removeContact}
        disclosureIndicator={false}
        sectionIndexTitles={titles}
        ListFooterComponent={
          <ListFooter text={`${contacts.data.length} travellers`} />
        }
      />
    </View>
  );
};
