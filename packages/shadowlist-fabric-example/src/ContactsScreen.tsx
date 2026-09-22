import { useRef } from 'react';
import { View } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import { Contacts, ListFooter } from 'shadowlist-utils/native';
import { useScreenStyles } from './screenStyles';
import { useHeaderActions } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { useContactActions } from './contactActions';
import { useAddContacts, useContactsQuery } from './queries/contacts';

export const ContactsScreen = () => {
  const styles = useScreenStyles();
  const shadowlistRef = useRef<ShadowListCommands>(null);

  const contacts = useContactsQuery();
  const { mutate: addContacts } = useAddContacts();
  const { openContact, removeContact } = useContactActions();

  useHeaderActions({
    onPrepend: () => addContacts({ count: 10, position: 'start' }),
    onAppend: () => addContacts({ count: 10, position: 'end' }),
    onScrollToRandom: () =>
      shadowlistRef.current?.scrollToIndex(
        Math.floor(Math.random() * (contacts.data?.length ?? 0))
      ),
    prependLabel: 'Add Companions to Top',
    appendLabel: 'Add Companions to Bottom',
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
        onPressItem={openContact}
        onDelete={removeContact}
        ListFooterComponent={
          <ListFooter text={`${contacts.data.length} companions`} />
        }
      />
    </View>
  );
};
