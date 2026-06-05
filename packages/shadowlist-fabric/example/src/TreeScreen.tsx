import { useRef, useState, useMemo, useCallback } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import { TreeList, type TreeListCommands } from 'shadowlist';
import { HeaderListItem } from './HeaderListItem';
import { TreeElement, type TreeFileNode } from './TreeElement';
import { generateFileTree } from './constants';

/*
 * Collect every folder id in the tree (an expanded folder is one whose id is in the
 * set). Used by "Expand all"; "Collapse all" is just the empty set.
 */
const collectFolderIds = (nodes: TreeFileNode[]): string[] => {
  const ids: string[] = [];
  const stack = [...nodes];
  while (stack.length > 0) {
    const node = stack.pop() as TreeFileNode;
    if (node.children && node.children.length > 0) {
      ids.push(node.id);
      stack.push(...node.children);
    }
  }
  return ids;
};

const getChildren = (node: TreeFileNode) => node.children;
const keyExtractor = (node: TreeFileNode) => node.id;

export const TreeScreen = () => {
  const treeRef = useRef<TreeListCommands>(null);

  const tree = useMemo(() => generateFileTree(), []);
  const allFolderIds = useMemo(() => collectFolderIds(tree), [tree]);

  /*
   * Controlled expansion: start with the root folders open so the screen is not a
   * wall of collapsed roots. Toggling, expand-all and collapse-all all flow through
   * this one Set; TreeList re-flattens and the engine reconciles by key.
   */
  const [expandedIds, setExpandedIds] = useState<Set<string>>(
    () => new Set(tree.map((node) => node.id))
  );

  const expandAll = useCallback(
    () => setExpandedIds(new Set(allFolderIds)),
    [allFolderIds]
  );
  const collapseAll = useCallback(() => setExpandedIds(new Set()), []);

  const renderNode = useCallback(
    ({
      item,
      depth,
      indent,
      isExpanded,
      hasChildren,
      toggle,
    }: {
      item: TreeFileNode;
      depth: number;
      indent: number;
      isExpanded: boolean;
      hasChildren: boolean;
      toggle: () => void;
    }) => (
      <TreeElement
        element={item}
        depth={depth}
        indent={indent}
        isExpanded={isExpanded}
        hasChildren={hasChildren}
        onToggle={toggle}
      />
    ),
    []
  );

  return (
    <View style={styles.container}>
      <TreeList<TreeFileNode>
        ref={treeRef}
        data={tree}
        getChildren={getChildren}
        keyExtractor={keyExtractor}
        renderNode={renderNode}
        expandedIds={expandedIds}
        onExpandedChange={setExpandedIds}
        style={styles.list}
        ListHeaderComponent={
          <HeaderListItem title="Files" subtitle="Virtualized directory tree" />
        }
      />
      <View style={styles.bar}>
        <Pressable style={styles.button} onPress={expandAll}>
          <Text style={styles.buttonText}>Expand all</Text>
        </Pressable>
        <Pressable style={styles.button} onPress={collapseAll}>
          <Text style={styles.buttonText}>Collapse all</Text>
        </Pressable>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#000000',
  },
  list: {
    flex: 1,
    backgroundColor: '#000000',
  },
  bar: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    paddingVertical: 12,
    paddingHorizontal: 16,
    backgroundColor: '#111111',
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: '#2F3336',
  },
  button: {
    paddingVertical: 8,
    paddingHorizontal: 20,
    borderRadius: 8,
    backgroundColor: '#FF9500',
  },
  buttonText: {
    color: '#000000',
    fontWeight: '700',
    fontSize: 14,
  },
});
