import { memo, useCallback, useMemo, type ReactNode } from 'react';
import {
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
import { createStyles, useTheme } from '../theme';

export interface GroupedRowProps {
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

// A card row that leads somewhere: a title over a subtitle, then a chevron.
export const GroupedRow = memo(
  ({
    title,
    subtitle,
    subtitleLines = 1,
    leading,
    trailing,
    chevron = true,
    onPress,
    onLongPress,
    longPressLabel,
    disabled = false,
    separated = false,
    accessibilityRole = 'button',
    style,
    testID,
  }: GroupedRowProps) => {
    const styles = useStyles();
    const { colors } = useTheme();
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
    return (
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
        onPress={onPress}
        onLongPress={onLongPress}
        style={({ pressed }) => [
          styles.row,
          pressed && styles.pressed,
          disabled && styles.disabled,
          style,
        ]}
      >
        {leading}
        <View style={[styles.body, separated && styles.separated]}>
          <View style={styles.text}>
            <Text style={styles.title} numberOfLines={largeText ? 2 : 1}>
              {title}
            </Text>
            {subtitle !== undefined ? (
              <Text style={styles.subtitle} numberOfLines={subtitleLines}>
                {subtitle}
              </Text>
            ) : null}
          </View>
          {trailing}
          {chevron ? (
            <ChevronIcon size={16} color={colors.tertiaryLabel} />
          ) : null}
        </View>
      </Pressable>
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
