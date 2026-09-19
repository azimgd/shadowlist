import { useRef } from 'react';
import { View } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import { Contacts, ListHeader, ListFooter } from 'shadowlist-utils/native';
import { useScreenStyles } from './screenStyles';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { showContact } from './fixtures/contacts';
import {
  useAddContacts,
  useContactsQuery,
  useDeleteContact,
} from './queries/contacts';

export const ContactsScreen = () => {
  const styles = useScreenStyles();
  const shadowlistRef = useRef<ShadowListCommands>(null);

  const contacts = useContactsQuery();
  const { mutate: addContacts } = useAddContacts();
  const { mutate: deleteContact } = useDeleteContact();

  useHeaderActions({
    onPrepend: () => addContacts({ count: 10, position: 'start' }),
    onAppend: () => addContacts({ count: 10, position: 'end' }),
    onScrollToRandom: () =>
      shadowlistRef.current?.scrollToIndex(
        Math.floor(Math.random() * (contacts.data?.length ?? 0))
      ),
  });

  if (contacts.data === undefined) {
    return <QueryStatus error={contacts.error} onRetry={contacts.refetch} />;
  }

  return (
    <View style={styles.container}>
      <Contacts.List
        data={contacts.data}
        ref={shadowlistRef}
        style={styles.list}
        onPressItem={showContact}
        onDelete={deleteContact}
        ListHeaderComponent={
          <ListHeader title="Companions" subtitle="Swipe left to remove" />
        }
        ListFooterComponent={
          <ListFooter text={`${contacts.data.length} companions`} />
        }
      />
    </View>
  );
};
