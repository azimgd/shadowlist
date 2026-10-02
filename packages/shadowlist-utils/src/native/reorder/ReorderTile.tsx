import { memo, useCallback, useMemo } from 'react';
import {
  StyleSheet,
  Text,
  View,
  type AccessibilityActionEvent,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { useLabels } from '../labels';
import { useLargeText } from '../internal/useLargeText';
import { getAvatarColor } from '../primitives/avatarAppearance';
import { createStyles, useTheme } from '../theme';
import { defaultReorderLabels, type ReorderLabels } from './labels';
import type { ReorderTileItem } from './types';

export interface ReorderTileProps {
  item: ReorderTileItem;
  // Moves are also offered as accessibility actions because screen readers cannot drag.
  onMove?: (id: string, offset: -1 | 1) => void;
  // Overrides the item's swatch ratio. Every tile in the grid is the same size.
  aspectRatio?: number;
  labels?: Partial<ReorderLabels>;
  style?: StyleProp<ViewStyle>;
}

export const ReorderTile = memo(
  ({ item, onMove, aspectRatio, labels, style }: ReorderTileProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const largeText = useLargeText();
    const l = useLabels(defaultReorderLabels, labels);
    const evenSize = aspectRatio !== undefined;

    const accessibilityActions = useMemo(
      () =>
        onMove !== undefined
          ? [
              { name: 'moveEarlier', label: l.moveEarlier },
              { name: 'moveLater', label: l.moveLater },
            ]
          : undefined,
      [onMove, l]
    );
    const handleAccessibilityAction = useCallback(
      (event: AccessibilityActionEvent) => {
        const { actionName } = event.nativeEvent;
        if (actionName === 'moveEarlier' || actionName === 'moveLater') {
          onMove?.(item.id, actionName === 'moveEarlier' ? -1 : 1);
        }
      },
      [onMove, item.id]
    );

    const color =
      item.color ??
      getAvatarColor(item.label ?? item.title, theme.colors.avatarPalette);

    return (
      <View
        style={[styles.tile, style]}
        accessible
        accessibilityLabel={
          item.label !== undefined ? `${item.label}, ${item.title}` : item.title
        }
        accessibilityHint={l.dragHint}
        accessibilityActions={accessibilityActions}
        onAccessibilityAction={handleAccessibilityAction}
      >
        <View
          style={[
            styles.swatch,
            {
              aspectRatio: aspectRatio ?? item.aspectRatio ?? 1,
              backgroundColor: color,
            },
          ]}
        >
          {item.label !== undefined && (
            <Text style={styles.label} allowFontScaling={false}>
              {item.label}
            </Text>
          )}
        </View>
        <Text
          style={styles.title}
          // Even tiles keep one line so every row is the same height.
          numberOfLines={evenSize ? 1 : largeText ? 4 : 2}
        >
          {item.title}
        </Text>
      </View>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    tile: {
      backgroundColor: theme.colors.background,
      paddingHorizontal: 6,
      paddingBottom: theme.spacing.md,
    },
    swatch: {
      width: '100%',
      borderRadius: theme.radius.md,
      alignItems: 'center',
      justifyContent: 'center',
      marginBottom: theme.spacing.xs,
    },
    label: {
      color: '#FFFFFF',
      ...theme.typography.title2,
    },
    title: {
      color: theme.colors.label,
      ...theme.typography.footnote,
      paddingHorizontal: theme.spacing.xs,
    },
  })
);
