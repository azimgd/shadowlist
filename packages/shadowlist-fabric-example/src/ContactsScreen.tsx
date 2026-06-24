import { useCallback, useMemo, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { type ShadowListCommands } from 'shadowlist';
import {
  Contacts,
  ListHeader,
  ListFooter,
  colors,
} from 'shadowlist-utils/native';
import {
  generateContact,
  useListController,
  type ContactItem,
} from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

export const ContactsScreen = () => {
  const shadowlistRef = useRef<ShadowListCommands>(null);
  const initialData = useMemo(
    () => Array.from({ length: 100 }, (_, index) => generateContact(index)),
    []
  );
  const list = useListController<ContactItem>({ initialData });
  const { removeItems } = list;

  const handlePrepend = () =>
    list.prepend(
      Array.from({ length: 10 }, (_, index) =>
        generateContact(list.data.length + index)
      )
    );
  const handleAppend = () =>
    list.append(
      Array.from({ length: 10 }, (_, index) =>
        generateContact(list.data.length + index)
      )
    );
  const handleScrollToRandom = () =>
    shadowlistRef.current?.scrollToIndex(
      Math.floor(Math.random() * list.data.length)
    );

  useHeaderActions({
    onPrepend: handlePrepend,
    onAppend: handleAppend,
    onScrollToRandom: handleScrollToRandom,
  });

  const handleDelete = useCallback(
    (key: string) => removeItems([key]),
    [removeItems]
  );

  const renderElement = useCallback(
    ({ element }: { element: ContactItem }) => (
      <Contacts.Row element={element} onDelete={handleDelete} />
    ),
    [handleDelete]
  );

  return (
    <View style={styles.container}>
      <Contacts.List
        data={list.data}
        ref={shadowlistRef}
        style={styles.list}
        renderElement={renderElement}
        ListHeaderComponent={
          <ListHeader title="Contacts" subtitle="Swipe left to delete" />
        }
        ListFooterComponent={
          <ListFooter text={`${list.data.length} contacts`} />
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
