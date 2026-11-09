import { useCallback, useState } from 'react';
import { Pressable, StyleSheet, Text } from 'react-native';
import { useLabels } from '../labels';
import { createStyles } from '../theme';
import type { ContactRowProps } from './ContactRow';
import { ContactRowContent } from './ContactRowContent';
import { defaultContactsLabels } from './labels';

export type SwipeableContactRowProps = ContactRowProps & {
  onDelete: (id: string) => void;
};

/*
 * A Mac has no swipe to reveal. Pointing at the row shows its delete button instead, the way
 * Mail shows row actions on hover. The accessibility delete action stays as on iOS.
 */
export const SwipeableContactRow = ({
  item,
  onPress,
  onDelete,
  disclosureIndicator = true,
  labels,
  style,
  avatarStyle,
}: SwipeableContactRowProps) => {
  const styles = useStyles();
  const { delete: deleteLabel } = useLabels(defaultContactsLabels, labels);
  const [hovered, setHovered] = useState(false);

  const handleDelete = useCallback(
    () => onDelete(item.id),
    [onDelete, item.id]
  );
  const handlePress = useCallback(() => onPress?.(item), [onPress, item]);

  return (
    <Pressable
      accessible={false}
      onHoverIn={() => setHovered(true)}
      onHoverOut={() => setHovered(false)}
    >
      <ContactRowContent
        item={item}
        onPress={handlePress}
        isPressable={onPress !== undefined}
        disclosureIndicator={disclosureIndicator && !hovered}
        onDelete={handleDelete}
        deleteLabel={deleteLabel}
        style={style}
        avatarStyle={avatarStyle}
      />
      {hovered ? (
        <Pressable
          onPress={handleDelete}
          accessibilityElementsHidden
          importantForAccessibility="no-hide-descendants"
          style={({ pressed }) => [
            styles.deleteButton,
            pressed && styles.pressed,
          ]}
        >
          <Text style={styles.deleteText} numberOfLines={1}>
            {deleteLabel}
          </Text>
        </Pressable>
      ) : null}
    </Pressable>
  );
};

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    deleteButton: {
      position: 'absolute',
      right: theme.spacing.md,
      top: '50%',
      height: 24,
      marginTop: -12,
      paddingHorizontal: theme.spacing.md,
      borderRadius: theme.radius.sm,
      justifyContent: 'center',
      backgroundColor: theme.colors.red,
    },
    deleteText: {
      color: theme.colors.onAccent,
      ...theme.typography.subhead,
      fontWeight: theme.fontWeight.semibold,
    },
    pressed: {
      opacity: 0.7,
    },
  })
);
