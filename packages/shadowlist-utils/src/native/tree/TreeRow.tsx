import { memo, useCallback, type ReactNode } from 'react';
import {
  Pressable,
  StyleSheet,
  Text,
  View,
  type StyleProp,
  type ViewStyle,
} from 'react-native';
import { ChevronIcon, DocIcon, FolderIcon } from '../icons';
import { useLabels } from '../labels';
import { useLargeText } from '../internal/useLargeText';
import { createStyles, useTheme } from '../theme';
import { defaultTreeLabels, type TreeLabels } from './labels';
import type { TreeNode } from './types';

export interface TreeRowProps {
  item: TreeNode;
  indent: number;
  isExpanded: boolean;
  hasChildren: boolean;
  onToggle: () => void;
  // Called for rows without children; rows with children toggle instead.
  onPress?: (item: TreeNode) => void;
  // Replaces the folder or file icon.
  icon?: ReactNode;
  labels?: Partial<TreeLabels>;
  style?: StyleProp<ViewStyle>;
}

export const TreeRow = memo(
  ({
    item,
    indent,
    isExpanded,
    hasChildren,
    onToggle,
    onPress,
    icon,
    labels,
    style,
  }: TreeRowProps) => {
    const theme = useTheme();
    const styles = useStyles();
    const largeText = useLargeText();
    const l = useLabels(defaultTreeLabels, labels);
    const isFolder =
      (item.kind ?? (item.children ? 'folder' : 'file')) === 'folder';

    const handlePress = useCallback(() => onPress?.(item), [onPress, item]);
    const pressHandler = hasChildren
      ? onToggle
      : onPress !== undefined
        ? handlePress
        : undefined;

    return (
      <Pressable
        style={({ pressed }) => [
          styles.row,
          pressed && pressHandler !== undefined && styles.pressed,
          style,
        ]}
        onPress={pressHandler}
        accessibilityRole={pressHandler !== undefined ? 'button' : undefined}
        accessibilityLabel={item.name}
        accessibilityHint={
          hasChildren ? (isExpanded ? l.collapse : l.expand) : undefined
        }
        accessibilityState={hasChildren ? { expanded: isExpanded } : undefined}
      >
        <View style={{ width: indent }} />
        <View style={styles.chevron}>
          {hasChildren ? (
            <ChevronIcon
              direction={isExpanded ? 'down' : 'right'}
              color={theme.colors.tertiaryLabel}
              size={14}
              strokeWidth={1.75}
            />
          ) : null}
        </View>
        <View style={styles.glyph}>
          {icon ??
            (isFolder ? <FolderIcon size={20} /> : <DocIcon size={18} />)}
        </View>
        <Text style={styles.name} numberOfLines={largeText ? undefined : 1}>
          {item.name}
        </Text>
        {isFolder && item.children ? (
          <Text style={styles.count}>{item.children.length}</Text>
        ) : null}
      </Pressable>
    );
  }
);

const useStyles = createStyles((theme) =>
  StyleSheet.create({
    row: {
      flexDirection: 'row',
      alignItems: 'center',
      minHeight: 44,
      paddingVertical: theme.spacing.xs,
      paddingRight: theme.spacing.lg,
      backgroundColor: theme.colors.background,
    },
    pressed: {
      backgroundColor: theme.colors.elevated,
    },
    chevron: {
      width: 22,
      alignItems: 'center',
      justifyContent: 'center',
    },
    glyph: {
      width: 24,
      alignItems: 'center',
      marginRight: theme.spacing.sm,
    },
    name: {
      flex: 1,
      color: theme.colors.label,
      ...theme.typography.callout,
    },
    count: {
      color: theme.colors.tertiaryLabel,
      ...theme.typography.footnote,
      marginLeft: theme.spacing.sm,
    },
  })
);
