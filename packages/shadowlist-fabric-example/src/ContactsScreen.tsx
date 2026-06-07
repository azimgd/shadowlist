import { useState, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { type ShadowlistCommands } from 'shadowlist';
import {
  Contacts,
  ListHeader,
  ListFooter,
  colors,
} from 'shadowlist-utils/native';
import { generateContact, type ContactItem } from 'shadowlist-utils';
import { useHeaderActions } from './HeaderActions';

export const ContactsScreen = () => {
  const shadowlistRef = useRef<ShadowlistCommands>(null);
  const [data, setData] = useState<ContactItem[]>(() =>
    Array.from({ length: 100 }, (_, index) => generateContact(index))
  );

  const handlePrepend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateContact(currentLength + index)
    );
    setData((prev) => [...newElements, ...prev]);
  };

  const handleAppend = () => {
    const currentLength = data.length;
    const newElements = Array.from({ length: 10 }, (_, index) =>
      generateContact(currentLength + index)
    );
    setData((prev) => [...prev, ...newElements]);
  };

  const handleScrollToRandom = () => {
    shadowlistRef.current?.scrollToIndex(
      Math.floor(Math.random() * data.length)
    );
  };

  useHeaderActions({
    onPrepend: handlePrepend,
    onAppend: handleAppend,
    onScrollToRandom: handleScrollToRandom,
  });

  const handleDelete = (id: string) => {
    setData((prev) => prev.filter((contact) => contact.id !== id));
  };

  return (
    <View style={styles.container}>
      <Contacts.List
        data={data}
        ref={shadowlistRef}
        style={styles.list}
        renderElement={({ element, index }) => (
          <Contacts.Row element={element} index={index} onDelete={handleDelete} />
        )}
        ListHeaderComponent={
          <ListHeader title="Contacts" subtitle="Swipe left to delete" />
        }
        ListFooterComponent={<ListFooter text={`${data.length} contacts`} />}
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
