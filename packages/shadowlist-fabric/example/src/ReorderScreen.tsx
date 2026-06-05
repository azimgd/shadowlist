import { memo, useState } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { Shadowlist } from 'shadowlist';
import { HeaderListItem } from './HeaderListItem';
import { AVATAR_COLORS, generateContact } from './constants';
import type { ContactElement as ContactElementType } from './ContactElement';

/*
 * A deliberately plain row (no reanimated, no gesture-handler): the long-press drag,
 * finger tracking, auto-scroll and shuffle are all handled natively by Shadowlist via
 * the dragEnabled prop. The row only renders content.
 */
/*
 * Derive a stable color from the contact's id (not its row index) so a row keeps its
 * avatar color when it is dragged to a new position.
 */
const colorForId = (id: string) => {
  let hash = 0;
  for (let i = 0; i < id.length; i++) hash = (hash * 31 + id.charCodeAt(i)) | 0;
  return AVATAR_COLORS[Math.abs(hash) % AVATAR_COLORS.length];
};

const ReorderElement = memo(
  ({ element }: { element: ContactElementType; index: number }) => {
    const avatarColor = colorForId(element.id);
    const initials = `${element.firstName.charAt(0)}${element.lastName.charAt(0)}`;

    return (
      <View style={styles.row}>
        <View style={[styles.avatar, { backgroundColor: avatarColor }]}>
          <Text style={styles.initials}>{initials}</Text>
        </View>
        <View style={styles.rowText}>
          <Text style={styles.name}>
            {element.firstName} {element.lastName}
          </Text>
          <Text style={styles.phone}>{element.phoneNumber}</Text>
        </View>
        <Text style={styles.grip}>≡</Text>
      </View>
    );
  }
);

export const ReorderScreen = () => {
  const [data, setData] = useState<ContactElementType[]>(() =>
    Array.from({ length: 80 }, (_, index) => generateContact(index))
  );

  return (
    <View style={styles.container}>
      <Shadowlist
        data={data}
        style={styles.list}
        dragEnabled
        onReorder={({ data: reordered }) => setData(reordered)}
        renderElement={({ element, index }) => (
          <ReorderElement element={element} index={index} />
        )}
        ListHeaderComponent={
          <HeaderListItem
            title="Reorder"
            subtitle="Press and hold a row, then drag to reorder"
          />
        }
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
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingHorizontal: 16,
    paddingVertical: 12,
    backgroundColor: '#000000',
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#1C1C1E',
  },
  avatar: {
    width: 44,
    height: 44,
    borderRadius: 22,
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: 12,
  },
  initials: {
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: '600',
  },
  rowText: {
    flex: 1,
  },
  name: {
    color: '#FFFFFF',
    fontSize: 16,
    fontWeight: '500',
  },
  phone: {
    color: '#8E8E93',
    fontSize: 13,
    marginTop: 2,
  },
  grip: {
    color: '#48484A',
    fontSize: 22,
    paddingHorizontal: 8,
  },
});
