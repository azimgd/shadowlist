import { memo } from 'react';
import {
  StyleSheet,
  Text,
  View,
  type ColorValue,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { createStyles } from '../theme';

export interface BoardCardProps {
  title: string;
  label?: string;
  detail?: string;
  color?: ColorValue;
  accessibilityLabel?: string;
  style?: StyleProp<ViewStyle>;
  testID?: string;
}

/*
 * A card in a board column: a title over an optional label and detail, with a bar and a dot
 * in color. Its cell stays transparent and the column paints behind it. A card picked up then
 * lifts as the rounded card alone, not a slab of the column's color.
 */
export const BoardCard = memo(
  ({
    title,
    label,
    detail,
    color,
    accessibilityLabel,
    style,
    testID,
  }: BoardCardProps) => {
    const styles = useStyles();
    const hasMeta = label !== undefined || detail !== undefined;
    return (
      <View style={styles.cell}>
        <View
          style={[styles.card, style]}
          testID={testID}
          accessible
          accessibilityLabel={
            accessibilityLabel ??
            [title, label, detail].filter(Boolean).join(', ')
          }
        >
          {color !== undefined ? (
            <View style={[styles.accent, { backgroundColor: color }]} />
          ) : null}
          <View style={styles.body}>
            <Text style={styles.title}>{title}</Text>
            {hasMeta ? (
              <View style={styles.meta}>
                {color !== undefined && label !== undefined ? (
                  <View style={[styles.dot, { backgroundColor: color }]} />
                ) : null}
                <Text style={styles.label}>{label}</Text>
                {detail !== undefined ? (
                  <Text style={styles.detail}>{detail}</Text>
                ) : null}
              </View>
            ) : null}
          </View>
        </View>
      </View>
    );
  }
);

const useStyles = createStyles(
  ({ colors, spacing, radius, typography, fontWeight }) =>
    StyleSheet.create({
      cell: {
        paddingHorizontal: spacing.md,
        paddingBottom: spacing.sm,
      },
      card: {
        flexDirection: 'row',
        backgroundColor: colors.background,
        borderRadius: radius.md,
        borderWidth: StyleSheet.hairlineWidth,
        borderColor: colors.separator,
        overflow: 'hidden',
      },
      accent: {
        width: 4,
      },
      body: {
        flex: 1,
        paddingHorizontal: spacing.md,
        paddingVertical: spacing.md,
        gap: spacing.xs,
      },
      title: {
        ...typography.subhead,
        color: colors.label,
      },
      meta: {
        flexDirection: 'row',
        alignItems: 'center',
        gap: spacing.xs,
      },
      dot: {
        width: 8,
        height: 8,
        borderRadius: 4,
      },
      label: {
        ...typography.caption,
        fontWeight: fontWeight.semibold,
        color: colors.secondaryLabel,
        flex: 1,
      },
      detail: {
        ...typography.caption,
        color: colors.tertiaryLabel,
        fontVariant: ['tabular-nums'],
      },
    })
);
