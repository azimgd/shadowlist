import { useMemo, useRef } from 'react';
import { Pressable, StyleSheet, Text, View } from 'react-native';
import { SectionList, type SectionListCommands } from 'shadowlist';
import { groupIntoSections } from 'shadowlist-utils';
import {
  Avatar,
  ListFooter,
  SectionHeader,
  createStyles,
} from 'shadowlist-utils/native';
import { useChangeTravellers, useTravellersQuery } from '../queries/travellers';
import { useHeaderMenu } from './HeaderActions';
import { QueryStatus } from './QueryStatus';
import { useScreenStyles } from './screenStyles';

export const SectionsScreen = () => {
  const styles = useScreenStyles();
  const rowStyles = useStyles();
  const list = useRef<SectionListCommands>(null);
  const fresh = useRef(5000);
  const travellers = useTravellersQuery('Sections');
  const { mutate: addTraveller } = useChangeTravellers('Sections');
  const sections = useMemo(() => {
    const contacts = [...(travellers.data ?? [])].sort((a, b) =>
      a.name.localeCompare(b.name)
    );
    return groupIntoSections(
      contacts.map((contact, index) => ({
        ...contact,
        subtitle:
          index % 5 === 0
            ? `${contact.subtitle} · Window seat, travelling with two bags and a guitar case that goes in the cabin.`
            : contact.subtitle,
      })),
      { getSectionTitle: (contact) => contact.name.charAt(0).toUpperCase() }
    );
  }, [travellers.data]);

  useHeaderMenu([
    [
      {
        label: 'Add Traveller',
        symbol: 'plus',
        onPress: () => addTraveller(fresh.current++),
      },
    ],
  ]);

  if (travellers.data === undefined) {
    return (
      <QueryStatus error={travellers.error} onRetry={travellers.refetch} />
    );
  }

  return (
    <View style={styles.container}>
      <SectionList
        ref={list}
        sections={sections}
        style={styles.list}
        stickySectionHeadersEnabled
        sectionIndexTitles={sections.map((section) => section.title)}
        renderSectionHeader={({ section }) => (
          <Pressable
            accessibilityRole="button"
            onPress={() =>
              list.current?.scrollToSection(
                (sections.indexOf(section) + 1) % sections.length,
                true
              )
            }
          >
            <SectionHeader
              title={`${section.title} · ${section.data.length} travellers · Next section`}
            />
          </Pressable>
        )}
        renderSectionFooter={({ section }) => (
          <ListFooter text={`${section.data.length} travellers`} />
        )}
        renderItem={({ item }) => (
          <View
            style={rowStyles.row}
            accessible
            accessibilityLabel={`${item.name}, ${item.subtitle}`}
          >
            <Avatar name={item.name} />
            <View style={rowStyles.body}>
              <Text style={rowStyles.name}>{item.name}</Text>
              <Text style={rowStyles.note}>{item.subtitle}</Text>
            </View>
            <View style={rowStyles.separator} />
          </View>
        )}
      />
    </View>
  );
};

const useStyles = createStyles(({ colors, spacing, typography, rowInset }) =>
  StyleSheet.create({
    row: {
      flexDirection: 'row',
      alignItems: 'flex-start',
      paddingLeft: spacing.lg,
      paddingRight: 36,
      paddingVertical: 11,
      gap: spacing.md,
      backgroundColor: colors.background,
    },
    body: { flex: 1 },
    name: { ...typography.body, color: colors.label },
    note: { ...typography.subhead, color: colors.secondaryLabel },
    separator: {
      position: 'absolute',
      left: rowInset,
      right: 0,
      bottom: 0,
      height: StyleSheet.hairlineWidth,
      backgroundColor: colors.separator,
    },
  })
);
