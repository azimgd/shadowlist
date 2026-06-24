import { memo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { type ContactItem } from 'shadowlist-utils';
import {
  colors,
  typography,
  ROW_INSET,
  spacing,
  radius,
  fontSize,
  fontWeight,
} from '../theme';
import { Grip } from '../icons';

export interface ReorderRowProps {
  element: ContactItem;
}

export const ReorderRow = memo(({ element }: ReorderRowProps) => {
  const initials = `${element.firstName.charAt(0)}${element.lastName.charAt(0)}`;

  return (
    <View style={styles.row}>
      <View style={[styles.avatar, { backgroundColor: element.avatarColor }]}>
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
    paddingLeft: spacing.lg,
    paddingRight: spacing.lg,
    paddingVertical: spacing.md,
    backgroundColor: colors.background,
  },
  avatar: {
    width: 40,
    height: 40,
    borderRadius: radius.xl,
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: spacing.md,
  },
  initials: {
    color: colors.label,
    fontSize: fontSize.body,
    fontWeight: fontWeight.semibold,
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
