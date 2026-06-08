import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { AVATAR_COLORS, type ContactItem } from 'shadowlist-utils';
import { colors, typography, ROW_INSET } from '../theme';
import { Grip } from '../icons';

export interface ReorderRowProps {
  element: ContactItem;
}

// Stable color keyed by id so a row keeps its color when reordered.
const colorForId = (id: string) => {
  let hash = 0;
  // eslint-disable-next-line no-bitwise
  for (let i = 0; i < id.length; i++) hash = (hash * 31 + id.charCodeAt(i)) | 0;
  return AVATAR_COLORS[Math.abs(hash) % AVATAR_COLORS.length];
};

export const ReorderRow = memo(({ element }: ReorderRowProps) => {
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
      <Grip size={20} color={colors.tertiaryLabel} />
      <View style={styles.separator} />
    </View>
  );
});

const styles = StyleSheet.create({
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingLeft: 16,
    paddingRight: 16,
    paddingVertical: 12,
    backgroundColor: colors.background,
  },
  avatar: {
    width: 40,
    height: 40,
    borderRadius: 20,
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: 12,
  },
  initials: {
    color: colors.label,
    fontSize: 17,
    fontWeight: '600',
  },
  rowText: {
    flex: 1,
  },
  name: {
    color: colors.label,
    ...typography.body,
  },
  phone: {
    color: colors.secondaryLabel,
    ...typography.subhead,
    marginTop: 1,
  },
  separator: {
    position: 'absolute',
    left: ROW_INSET,
    right: 0,
    bottom: 0,
    height: StyleSheet.hairlineWidth,
    backgroundColor: colors.separator,
  },
});
