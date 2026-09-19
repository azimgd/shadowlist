import { memo, useCallback, useMemo } from 'react';
import {
  Pressable,
  StyleSheet,
  Text,
  View,
  type AccessibilityActionEvent,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { Gesture, GestureDetector } from 'react-native-gesture-handler';
import Animated, {
  useAnimatedStyle,
  useSharedValue,
  withSpring,
} from 'react-native-reanimated';
import { ChevronIcon } from '../icons';
import { useLabels } from '../labels';
import { createStyles, useTheme } from '../theme';
import { ContactBody } from './ContactBody';
import { getContactAccessibilityLabel } from './getContactAccessibilityLabel';
import { defaultContactsLabels, type ContactsLabels } from './labels';
import type { ContactItem } from './types';

export interface ContactRowProps {
  item: ContactItem;
  onPress?: (item: ContactItem) => void;
  // Enables swipe-to-delete and the matching accessibility action.
  onDelete?: (id: string) => void;
  labels?: Partial<ContactsLabels>;
  style?: StyleProp<ViewStyle>;
  avatarStyle?: StyleProp<ViewStyle>;
}

const DELETE_WIDTH = 80;
const OPEN_THRESHOLD = -80;
const OPEN_VELOCITY = -500;
const SPRING = { damping: 30, stiffness: 400, overshootClamping: true };

interface RowContentProps {
  item: ContactItem;
  onPress?: () => void;
  isPressable: boolean;
  onDelete?: () => void;
  deleteLabel?: string;
  style?: StyleProp<ViewStyle>;
  avatarStyle?: StyleProp<ViewStyle>;
}

const RowContent = ({
  item,
  onPress,
  isPressable,
  onDelete,
  deleteLabel,
  style,
  avatarStyle,
}: RowContentProps) => {
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
      {isPressable ? (
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

const StaticContactRow = ({
  item,
  onPress,
  style,
  avatarStyle,
}: Omit<ContactRowProps, 'onDelete' | 'labels'>) => {
  const handlePress = useCallback(() => onPress?.(item), [onPress, item]);
  return (
    <RowContent
      item={item}
      onPress={onPress !== undefined ? handlePress : undefined}
      isPressable={onPress !== undefined}
      style={style}
      avatarStyle={avatarStyle}
    />
  );
};

const SwipeableContactRow = ({
  item,
  onPress,
  onDelete,
  labels,
  style,
  avatarStyle,
}: ContactRowProps & { onDelete: (id: string) => void }) => {
  const styles = useStyles();
  const { delete: deleteLabel } = useLabels(defaultContactsLabels, labels);

  const translateX = useSharedValue(0);
  const startX = useSharedValue(0);

  const panGesture = useMemo(
    () =>
      Gesture.Pan()
        .activeOffsetX([-10, 10])
        .failOffsetY([-10, 10])
        .onStart(() => {
          startX.value = translateX.value;
        })
        .onChange((event) => {
          const next = startX.value + event.translationX;
          if (next <= 0) {
            translateX.value = next;
          }
        })
        .onEnd((event) => {
          const open =
            translateX.value < OPEN_THRESHOLD ||
            event.velocityX < OPEN_VELOCITY;
          translateX.value = withSpring(open ? -DELETE_WIDTH : 0, SPRING);
        }),
    [startX, translateX]
  );

  const rowStyle = useAnimatedStyle(() => ({
    transform: [{ translateX: translateX.value }],
  }));
  const deleteStyle = useAnimatedStyle(() => ({
    width: Math.abs(translateX.value),
    opacity: translateX.value < -5 ? 1 : 0,
  }));

  const handleDelete = useCallback(
    () => onDelete(item.id),
    [onDelete, item.id]
  );
  // A tap on an open row closes it instead of activating it.
  const handlePress = useCallback(() => {
    if (translateX.value !== 0) {
      translateX.value = withSpring(0, SPRING);
    } else {
      onPress?.(item);
    }
  }, [translateX, onPress, item]);

  return (
    <View style={styles.swipeContainer}>
      <Animated.View
        style={[styles.deleteAction, deleteStyle]}
        accessibilityElementsHidden
        importantForAccessibility="no-hide-descendants"
      >
        <Pressable style={styles.deleteButton} onPress={handleDelete}>
          <Text style={styles.deleteText} numberOfLines={1}>
            {deleteLabel}
          </Text>
        </Pressable>
      </Animated.View>
      <GestureDetector gesture={panGesture}>
        <Animated.View style={rowStyle}>
          <RowContent
            item={item}
            onPress={handlePress}
            isPressable={onPress !== undefined}
            onDelete={handleDelete}
            deleteLabel={deleteLabel}
            style={style}
            avatarStyle={avatarStyle}
          />
        </Animated.View>
      </GestureDetector>
    </View>
  );
};

export const ContactRow = memo((props: ContactRowProps) =>
  props.onDelete !== undefined ? (
    <SwipeableContactRow {...props} onDelete={props.onDelete} />
  ) : (
    <StaticContactRow
      item={props.item}
      onPress={props.onPress}
      style={props.style}
      avatarStyle={props.avatarStyle}
    />
  )
);

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
    swipeContainer: {
      overflow: 'hidden',
    },
    deleteAction: {
      position: 'absolute',
      top: 0,
      right: 0,
      bottom: 0,
      minWidth: DELETE_WIDTH,
      backgroundColor: theme.colors.red,
    },
    deleteButton: {
      flex: 1,
      alignItems: 'center',
      justifyContent: 'center',
    },
    deleteText: {
      color: theme.colors.onAccent,
      ...theme.typography.subhead,
      fontWeight: theme.fontWeight.semibold,
    },
  })
);
