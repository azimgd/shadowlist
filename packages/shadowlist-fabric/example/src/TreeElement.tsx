import { memo } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';

/* A file-system node for the TreeList example. */
export interface TreeFileNode {
  id: string;
  name: string;
  type: 'folder' | 'file';
  children?: TreeFileNode[];
}

interface TreeElementProps {
  element: TreeFileNode;
  depth: number;
  indent: number;
  isExpanded: boolean;
  hasChildren: boolean;
  onToggle: () => void;
}

/* One presentational row of the directory browser; a press toggles expansion. */
export const TreeElement = memo(
  ({
    element,
    indent,
    isExpanded,
    hasChildren,
    onToggle,
  }: TreeElementProps) => {
    const isFolder = element.type === 'folder';

    return (
      <Pressable
        style={styles.row}
        onPress={hasChildren ? onToggle : undefined}
      >
        <View style={[styles.indent, { width: indent }]} />
        <Text style={styles.chevron}>
          {hasChildren ? (isExpanded ? '▾' : '▸') : ' '}
        </Text>
        <Text style={styles.glyph}>{isFolder ? '📁' : '📄'}</Text>
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
    height: 40,
    paddingRight: 16,
    backgroundColor: '#000000',
  },
  indent: {
    height: '100%',
  },
  chevron: {
    width: 18,
    textAlign: 'center',
    color: '#FF9500',
    fontSize: 14,
  },
  glyph: {
    fontSize: 16,
    marginRight: 8,
  },
  name: {
    flex: 1,
    color: '#FFFFFF',
    fontSize: 15,
  },
  count: {
    color: '#666666',
    fontSize: 13,
    marginLeft: 8,
  },
});
