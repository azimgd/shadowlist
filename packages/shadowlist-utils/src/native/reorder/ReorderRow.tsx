import { memo, useCallback, useMemo } from 'react';
import {
  StyleSheet,
  View,
  type AccessibilityActionEvent,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { ContactBody } from '../contacts/ContactBody';
import { getContactAccessibilityLabel } from '../contacts/getContactAccessibilityLabel';
import type { ContactItem } from '../contacts/types';
import { GripIcon } from '../icons';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { defaultReorderLabels, type ReorderLabels } from './labels';

export interface ReorderRowProps {
  item: ContactItem;
  // Screen readers cannot drag, so moves are also offered as accessibility actions.
  onMove?: (id: string, offset: -1 | 1) => void;
  labels?: Partial<ReorderLabels>;
  style?: StyleProp<ViewStyle>;
  avatarStyle?: StyleProp<ViewStyle>;
}

export const ReorderRow = memo(
  ({ item, onMove, labels, style, avatarStyle }: ReorderRowProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const l = useLabels(defaultReorderLabels, labels);

    const accessibilityActions = useMemo(
      () =>
        onMove !== undefined
          ? [
              { name: 'moveUp', label: l.moveUp },
              { name: 'moveDown', label: l.moveDown },
            ]
          : undefined,
      [onMove, l]
    );
    const handleAccessibilityAction = useCallback(
      (event: AccessibilityActionEvent) => {
        const { actionName } = event.nativeEvent;
        if (actionName === 'moveUp' || actionName === 'moveDown') {
          onMove?.(item.id, actionName === 'moveUp' ? -1 : 1);
        }
      },
      [onMove, item.id]
    );

    return (
      <View
        style={[styles.row, style]}
        accessible
        accessibilityLabel={getContactAccessibilityLabel(item)}
        accessibilityHint={l.dragHint}
        accessibilityActions={accessibilityActions}
        onAccessibilityAction={handleAccessibilityAction}
      >
        <ContactBody contact={item} avatarStyle={avatarStyle} />
        <GripIcon size={20} color={theme.colors.tertiaryLabel} />
        <View style={styles.separator} />
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    row: {
      flexDirection: 'row',
      alignItems: 'center',
      paddingHorizontal: theme.spacing.lg,
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
