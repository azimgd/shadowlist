import { useCallback, useMemo } from 'react';
import { Pressable, StyleSheet, Text, View } from 'react-native';
import { Gesture, GestureDetector } from 'react-native-gesture-handler';
import Animated, {
  useAnimatedStyle,
  useSharedValue,
  withSpring,
} from 'react-native-reanimated';
import { useLabels } from '../labels';
import { createStyles } from '../theme';
import type { ContactRowProps } from './ContactRow';
import { ContactRowContent } from './ContactRowContent';
import { defaultContactsLabels } from './labels';

export type SwipeableContactRowProps = ContactRowProps & {
  onDelete: (id: string) => void;
};

const DELETE_WIDTH = 80;
const OPEN_THRESHOLD = -80;
const OPEN_VELOCITY = -500;
const SPRING = { damping: 30, stiffness: 400, overshootClamping: true };

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
          <ContactRowContent
            item={item}
            onPress={handlePress}
            isPressable={onPress !== undefined}
            disclosureIndicator={disclosureIndicator}
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

const useStyles = createStyles((theme) =>
  StyleSheet.create({
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
