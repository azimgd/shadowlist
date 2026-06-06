import { memo, useMemo } from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { Gesture, GestureDetector } from 'react-native-gesture-handler';
import Animated, {
  useSharedValue,
  useAnimatedStyle,
  withSpring,
} from 'react-native-reanimated';
import { AVATAR_COLORS } from './constants';
import { colors, typography, ROW_INSET } from './theme';
import { Chevron } from './icons';

export interface ContactElement {
  id: string;
  firstName: string;
  lastName: string;
  phoneNumber: string;
}

interface ContactElementProps {
  element: ContactElement;
  index: number;
}

const SWIPE_THRESHOLD = -80;
const DELETE_BUTTON_WIDTH = 80;

export const ContactElement = memo(
  ({ element, index }: ContactElementProps) => {
    const avatarColor = useMemo(() => {
      return AVATAR_COLORS[index % AVATAR_COLORS.length];
    }, [index]);

    const initials = useMemo(() => {
      return `${element.firstName.charAt(0)}${element.lastName.charAt(0)}`;
    }, [element.firstName, element.lastName]);

    const translateX = useSharedValue(0);
    const startX = useSharedValue(0);

    const panGesture = Gesture.Pan()
      .activeOffsetX([-10, 10])
      .failOffsetY([-10, 10])
      .onStart(() => {
        startX.value = translateX.value;
      })
      .onChange((event) => {
        const newTranslateX = startX.value + event.translationX;
        if (newTranslateX <= 0) {
          translateX.value = newTranslateX;
        }
      })
      .onEnd((event) => {
        if (translateX.value < SWIPE_THRESHOLD) {
          translateX.value = withSpring(-DELETE_BUTTON_WIDTH, {
            damping: 30,
            stiffness: 400,
            overshootClamping: true,
          });
        } else if (event.velocityX < -500) {
          translateX.value = withSpring(-DELETE_BUTTON_WIDTH, {
            damping: 30,
            stiffness: 400,
            overshootClamping: true,
          });
        } else {
          translateX.value = withSpring(0, {
            damping: 30,
            stiffness: 400,
            overshootClamping: true,
          });
        }
      });

    const wrapperStyle = useAnimatedStyle(() => {
      return {
        left: translateX.value,
      };
    });

    const deleteButtonStyle = useAnimatedStyle(() => {
      const width = Math.abs(translateX.value);
      return {
        width,
        opacity: translateX.value < -5 ? 1 : 0,
      };
    });

    return (
      <View style={styles.container}>
        <Animated.View style={[styles.deleteButton, deleteButtonStyle]}>
          <Text style={styles.deleteButtonText}>Delete</Text>
        </Animated.View>
        <GestureDetector gesture={panGesture}>
          <Animated.View style={[styles.wrapper, wrapperStyle]}>
            <View style={styles.contactElement}>
              <View style={[styles.avatar, { backgroundColor: avatarColor }]}>
                <Text style={styles.avatarText}>{initials}</Text>
              </View>
              <View style={styles.content}>
                <Text style={styles.name}>
                  {element.firstName} {element.lastName}
                </Text>
                <Text style={styles.phoneNumber}>{element.phoneNumber}</Text>
              </View>
              <Chevron
                direction="right"
                color={colors.tertiaryLabel}
                size={20}
                weight={2}
              />
              <View style={styles.separator} />
            </View>
          </Animated.View>
        </GestureDetector>
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    position: 'relative',
    overflow: 'hidden',
  },
  wrapper: {
    position: 'relative',
    left: 0,
  },
  contactElement: {
    backgroundColor: colors.background,
    paddingLeft: 16,
    paddingRight: 12,
    paddingVertical: 12,
    flexDirection: 'row',
    alignItems: 'center',
  },
  avatar: {
    width: 40,
    height: 40,
    borderRadius: 20,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
  },
  avatarText: {
    color: colors.label,
    fontSize: 17,
    fontWeight: '600',
  },
  content: {
    flex: 1,
  },
  name: {
    color: colors.label,
    ...typography.body,
    marginBottom: 1,
  },
  phoneNumber: {
    color: colors.secondaryLabel,
    ...typography.subhead,
  },
  separator: {
    position: 'absolute',
    left: ROW_INSET,
    right: 0,
    bottom: 0,
    height: StyleSheet.hairlineWidth,
    backgroundColor: colors.separator,
  },
  deleteButton: {
    position: 'absolute',
    right: 0,
    top: 0,
    bottom: 0,
    minWidth: DELETE_BUTTON_WIDTH,
    backgroundColor: colors.red,
    justifyContent: 'center',
    alignItems: 'center',
  },
  deleteButtonText: {
    color: colors.label,
    ...typography.subhead,
    fontWeight: '600',
  },
});
