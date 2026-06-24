import { useCallback, useMemo, useRef } from 'react';
import { View, StyleSheet } from 'react-native';
import { type ShadowListCommands, type SectionListData } from 'shadowlist';
import {
  SectionList,
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
  const sectionListRef = useRef<ShadowListCommands>(null);
  const initialData = useMemo(
    () => Array.from({ length: 300 }, (_, index) => generateContact(index)),
    []
  );
  const list = useListController<ContactItem>({ initialData });
  const { removeItems } = list;

  const sections = useMemo(() => buildSections(list.data), [list.data]);

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
    sectionListRef.current?.scrollToIndex(
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
      <SectionList.Row element={element} onDelete={handleDelete} />
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
