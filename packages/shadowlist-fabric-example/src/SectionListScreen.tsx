import { useCallback, useMemo, useRef } from 'react';
import { View } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import { groupIntoSections } from 'shadowlist-utils';
import {
  Contacts,
  ListFooter,
  type ContactItem,
} from 'shadowlist-utils/native';
import { useScreenStyles } from './screenStyles';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { SectionIndex } from './SectionIndex';
import { useContactActions } from './contactActions';
import { useAddContacts, useContactsQuery } from './queries/contacts';

const NO_CONTACTS: ContactItem[] = [];

const initialOf = (contact: ContactItem) =>
  (contact.name.charAt(0) || '#').toUpperCase();

const byName = (a: ContactItem, b: ContactItem) => a.name.localeCompare(b.name);

export const SectionListScreen = () => {
  const styles = useScreenStyles();
  const sectionListRef = useRef<ShadowListCommands>(null);

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

  const { titles, headerIndices } = useMemo(() => {
    let index = 0;
    const starts: number[] = [];
    for (const section of sections) {
      starts.push(index);
      index += 1 + section.data.length;
    }
    return {
      titles: sections.map((section) => section.title),
      headerIndices: starts,
    };
  }, [sections]);

  const jumpToSection = useCallback(
    (sectionIndex: number) => {
      const index = headerIndices[sectionIndex];
      if (index !== undefined) {
        sectionListRef.current?.scrollToIndex(index, 0);
      }
    },
    [headerIndices]
  );

  useHeaderActions({
    onPrepend: () => addContacts({ count: 10, position: 'start' }),
    onAppend: () => addContacts({ count: 10, position: 'end' }),
    onScrollToRandom: () =>
      sectionListRef.current?.scrollToIndex(
        Math.floor(Math.random() * (contacts.data?.length ?? 0))
      ),
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
        ListFooterComponent={
          <ListFooter text={`${contacts.data.length} travellers`} />
        }
      />
      <SectionIndex titles={titles} onSelect={jumpToSection} />
    </View>
  );
};
