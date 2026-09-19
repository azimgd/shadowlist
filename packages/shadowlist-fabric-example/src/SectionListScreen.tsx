import { useMemo, useRef } from 'react';
import { View } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import { groupIntoSections } from 'shadowlist-utils';
import {
  Contacts,
  ListHeader,
  ListFooter,
  type ContactItem,
} from 'shadowlist-utils/native';
import { useScreenStyles } from './screenStyles';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { showContact } from './fixtures/contacts';
import {
  useAddContacts,
  useContactsQuery,
  useDeleteContact,
} from './queries/contacts';

const NO_CONTACTS: ContactItem[] = [];

const initialOf = (contact: ContactItem) =>
  (contact.name.charAt(0) || '#').toUpperCase();

const byName = (a: ContactItem, b: ContactItem) => a.name.localeCompare(b.name);

export const SectionListScreen = () => {
  const styles = useScreenStyles();
  const sectionListRef = useRef<ShadowListCommands>(null);

  const contacts = useContactsQuery();
  const { mutate: addContacts } = useAddContacts();
  const { mutate: deleteContact } = useDeleteContact();

  const sections = useMemo(
    () =>
      groupIntoSections(contacts.data ?? NO_CONTACTS, {
        getSectionTitle: initialOf,
        compareItems: byName,
      }),
    [contacts.data]
  );

  useHeaderActions({
    onPrepend: () => addContacts({ count: 10, position: 'start' }),
    onAppend: () => addContacts({ count: 10, position: 'end' }),
    onScrollToRandom: () =>
      sectionListRef.current?.scrollToIndex(
        Math.floor(Math.random() * (contacts.data?.length ?? 0))
      ),
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
        onPressItem={showContact}
        onDelete={deleteContact}
        ListHeaderComponent={
          <ListHeader title="Directory" subtitle="Skyfy travellers, A to Z" />
        }
        ListFooterComponent={
          <ListFooter text={`${contacts.data.length} travellers`} />
        }
      />
    </View>
  );
};
