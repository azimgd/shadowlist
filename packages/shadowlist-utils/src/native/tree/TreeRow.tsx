import { memo } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import type { TreeFileNode } from 'shadowlist-utils';
import { colors, typography } from '../theme';
import { Chevron, Folder, Doc } from '../icons';

export interface TreeRowProps {
  element: TreeFileNode;
  depth: number;
  indent: number;
  isExpanded: boolean;
  hasChildren: boolean;
  onToggle: () => void;
}

/* One presentational row of the directory browser; a press toggles expansion. */
export const TreeRow = memo(
  ({ element, indent, isExpanded, hasChildren, onToggle }: TreeRowProps) => {
    const isFolder = element.type === 'folder';

    return (
      <Pressable
        style={({ pressed }) => [styles.row, pressed && styles.pressed]}
        onPress={hasChildren ? onToggle : undefined}
      >
        <View style={[styles.indent, { width: indent }]} />
        <View style={styles.chevron}>
          {hasChildren ? (
            <Chevron
              direction={isExpanded ? 'down' : 'right'}
              color={colors.tertiaryLabel}
              size={14}
              strokeWidth={1.75}
            />
          ) : null}
        </View>
        <View style={styles.glyph}>
          {isFolder ? <Folder size={20} /> : <Doc size={18} />}
        </View>
        <Text style={styles.name} numberOfLines={1}>
          {element.name}
        </Text>
        {isFolder && element.children ? (
          <Text style={styles.count}>{element.children.length}</Text>
        ) : null}
      </Pressable>
    );
  }
);

const styles = StyleSheet.create({
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    height: 44,
    paddingRight: 16,
    backgroundColor: colors.background,
  },
  pressed: {
    backgroundColor: colors.elevated,
  },
  indent: {
    height: '100%',
  },
  chevron: {
    width: 22,
    alignItems: 'center',
    justifyContent: 'center',
  },
  glyph: {
    width: 24,
    alignItems: 'center',
    marginRight: 8,
  },
  name: {
    flex: 1,
    color: colors.label,
    ...typography.callout,
  },
  count: {
    color: colors.tertiaryLabel,
    ...typography.footnote,
    marginLeft: 8,
  },
});
