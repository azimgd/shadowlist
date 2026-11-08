import { useCallback, useMemo } from 'react';
import {
  Pressable,
  StyleSheet,
  View,
  type AccessibilityActionEvent,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { ChevronIcon } from '../icons';
import { createStyles, useTheme } from '../theme';
import { ContactBody } from './ContactBody';
import { getContactAccessibilityLabel } from './getContactAccessibilityLabel';
import type { ContactItem } from './types';

export interface ContactRowContentProps {
  item: ContactItem;
  onPress?: () => void;
  isPressable: boolean;
  disclosureIndicator: boolean;
  onDelete?: () => void;
  deleteLabel?: string;
  style?: StyleProp<ViewStyle>;
  avatarStyle?: StyleProp<ViewStyle>;
}

export const ContactRowContent = ({
  item,
  onPress,
  isPressable,
  disclosureIndicator,
  onDelete,
  deleteLabel,
  style,
  avatarStyle,
}: ContactRowContentProps) => {
  const theme = useTheme();
  const styles = useStyles();

  const accessibilityActions = useMemo(
    () =>
      onDelete !== undefined && deleteLabel !== undefined
        ? [{ name: 'delete', label: deleteLabel }]
        : undefined,
    [onDelete, deleteLabel]
  );
  const handleAccessibilityAction = useCallback(
    (event: AccessibilityActionEvent) => {
      if (event.nativeEvent.actionName === 'delete') {
        onDelete?.();
      }
    },
    [onDelete]
  );

  return (
    <Pressable
      style={[styles.row, style]}
      onPress={onPress}
      accessible
      accessibilityRole={isPressable ? 'button' : undefined}
      accessibilityLabel={getContactAccessibilityLabel(item)}
      accessibilityActions={accessibilityActions}
      onAccessibilityAction={handleAccessibilityAction}
    >
      <ContactBody contact={item} avatarStyle={avatarStyle} />
      {isPressable && disclosureIndicator ? (
        <ChevronIcon
          direction="right"
          color={theme.colors.tertiaryLabel}
          size={20}
          strokeWidth={2}
        />
      ) : null}
      <View style={styles.separator} />
    </Pressable>
  );
};

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    row: {
      flexDirection: 'row',
      alignItems: 'center',
      paddingLeft: theme.spacing.lg,
      paddingRight: theme.spacing.md,
      paddingVertical: theme.spacing.md,
      backgroundColor: theme.colors.background,
    },
    separator: {
      position: 'absolute',
      left: theme.rowInset,
      right: 0,
      bottom: 0,
      height: StyleSheet.hairlineWidth,
      backgroundColor: theme.colors.separator,
    },
  })
);
