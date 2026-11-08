import {
  memo,
  useCallback,
  useEffect,
  useMemo,
  useRef,
  type ReactNode,
} from 'react';
import {
  Platform,
  Pressable,
  StyleSheet,
  Text,
  View,
  type AccessibilityActionEvent,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLargeText } from '../hooks/useLargeText';
import { ChevronIcon } from '../icons';
import { pressRippleFor } from '../internal/pressRipple';
import { createStyles, useTheme } from '../theme';
import { GroupedActions, type GroupedActionsProps } from './GroupedActions';

/*
 * A press held this long may be a menu on its way on iOS, where a finger lifted as the menu commits
 * ends the touch before the system says the menu is coming. It waits MENU_WAIT_MS to hear. A quick
 * tap opens at once.
 */
const MENU_POSSIBLE_AFTER_MS = 250;
const MENU_WAIT_MS = 150;

export interface GroupedRowProps extends Pick<
  GroupedActionsProps,
  'leadingSwipeActions' | 'trailingSwipeActions' | 'contextMenu'
> {
  title: string;
  subtitle?: string;
  subtitleLines?: number;
  leading?: ReactNode;
  trailing?: ReactNode;
  chevron?: boolean;
  onPress?: () => void;
  onLongPress?: () => void;
  longPressLabel?: string;
  disabled?: boolean;
  separated?: boolean;
  accessibilityRole?: 'button' | 'link';
  style?: StyleProp<ViewStyle>;
  testID?: string;
}

/*
 * A card row that leads somewhere: a title over a subtitle, then a chevron unless the theme's
 * grouped.chevron turns it off. Swipe actions or a menu put it in Grouped.Actions. At
 * accessibility text sizes the title wraps to three lines and the subtitle to at least three,
 * instead of truncating. A hold that opens the menu is not a press, even when the finger lifts
 * before the system has cancelled the touch (MENU_WAIT_MS).
 */
export const GroupedRow = memo(
  ({
    title,
    subtitle,
    subtitleLines = 1,
    leading,
    trailing,
    chevron,
    onPress,
    onLongPress,
    longPressLabel,
    disabled = false,
    separated = false,
    accessibilityRole = 'button',
    style,
    testID,
    leadingSwipeActions,
    trailingSwipeActions,
    contextMenu,
  }: GroupedRowProps) => {
    const styles = useStyles();
    const theme = useTheme();
    const { colors } = theme;
    const ripple = pressRippleFor(theme);
    const showsChevron = chevron ?? theme.grouped.chevron;
    const largeText = useLargeText();
    const accessibilityActions = useMemo(
      () =>
        onLongPress !== undefined && longPressLabel !== undefined
          ? [{ name: 'longpress', label: longPressLabel }]
          : undefined,
      [onLongPress, longPressLabel]
    );
    const handleAccessibilityAction = useCallback(
      (event: AccessibilityActionEvent) => {
        if (event.nativeEvent.actionName === 'longpress') {
          onLongPress?.();
        }
      },
      [onLongPress]
    );
    const isMenuHoldRef = useRef(false);
    const pressedAtRef = useRef(0);
    const waitRef = useRef<ReturnType<typeof setTimeout> | null>(null);
    const stopWaiting = useCallback(() => {
      if (waitRef.current !== null) {
        clearTimeout(waitRef.current);
        waitRef.current = null;
      }
    }, []);
    useEffect(() => stopWaiting, [stopWaiting]);
    const handlePressIn = useCallback(() => {
      stopWaiting();
      isMenuHoldRef.current = false;
      pressedAtRef.current = Date.now();
    }, [stopWaiting]);
    const handleMenuWillShow = useCallback(() => {
      isMenuHoldRef.current = true;
      stopWaiting();
    }, [stopWaiting]);
    const handlePress = useCallback(() => {
      if (isMenuHoldRef.current) {
        return;
      }
      if (
        Platform.OS !== 'ios' ||
        Date.now() - pressedAtRef.current < MENU_POSSIBLE_AFTER_MS
      ) {
        onPress?.();
        return;
      }
      waitRef.current = setTimeout(() => {
        waitRef.current = null;
        onPress?.();
      }, MENU_WAIT_MS);
    }, [onPress]);
    const row = (
      <Pressable
        testID={testID}
        accessibilityRole={accessibilityRole}
        accessibilityLabel={
          subtitle === undefined ? title : `${title}, ${subtitle}`
        }
        accessibilityState={{ disabled }}
        accessibilityActions={accessibilityActions}
        onAccessibilityAction={handleAccessibilityAction}
        disabled={disabled}
        onPressIn={contextMenu ? handlePressIn : undefined}
        onPress={contextMenu && onPress ? handlePress : onPress}
        onLongPress={onLongPress}
        android_ripple={ripple}
        style={({ pressed }) => [
          styles.row,
          pressed && ripple === undefined && styles.pressed,
          disabled && styles.disabled,
          style,
        ]}
      >
        {leading}
        <View style={[styles.body, separated && styles.separated]}>
          <View style={styles.text}>
            <Text style={styles.title} numberOfLines={largeText ? 3 : 1}>
              {title}
            </Text>
            {subtitle !== undefined ? (
              <Text
                style={styles.subtitle}
                numberOfLines={
                  largeText ? Math.max(subtitleLines, 3) : subtitleLines
                }
              >
                {subtitle}
              </Text>
            ) : null}
          </View>
          {trailing}
          {showsChevron ? (
            <ChevronIcon size={16} color={colors.tertiaryLabel} />
          ) : null}
        </View>
      </Pressable>
    );
    if (!leadingSwipeActions && !trailingSwipeActions && !contextMenu) {
      return row;
    }
    return (
      <GroupedActions
        leadingSwipeActions={leadingSwipeActions}
        trailingSwipeActions={trailingSwipeActions}
        contextMenu={contextMenu}
        onContextMenuWillShow={handleMenuWillShow}
      >
        {row}
      </GroupedActions>
    );
  }
);

const useStyles = createStyles(({ colors, typography, spacing, grouped }) =>
  StyleSheet.create({
    row: {
      flexDirection: 'row',
      alignItems: 'center',
      gap: spacing.md,
      paddingLeft: grouped.rowInset,
      backgroundColor: colors.groupedCell,
    },
    body: {
      flex: 1,
      flexDirection: 'row',
      alignItems: 'center',
      gap: spacing.sm,
      minHeight: grouped.rowHeight,
      paddingVertical: spacing.sm + 2,
      paddingRight: grouped.rowInset,
    },
    separated: {
      borderTopWidth: StyleSheet.hairlineWidth,
      borderTopColor: colors.separator,
    },
    text: {
      flex: 1,
      gap: 1,
    },
    title: {
      ...typography.body,
      color: colors.label,
    },
    subtitle: {
      ...typography.footnote,
      color: colors.secondaryLabel,
    },
    pressed: {
      backgroundColor: colors.fill,
    },
    disabled: {
      opacity: 0.4,
    },
  })
);
