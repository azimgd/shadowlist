import { useRef, useState, useMemo, useCallback } from 'react';
import { View, Text, Pressable, StyleSheet } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { type TreeListCommands } from 'shadowlist';
import { Tree, ListHeader, colors, typography } from 'shadowlist-utils/native';
import { generateFileTree, type TreeFileNode } from 'shadowlist-utils';

/* Collect every folder id in the tree; used by "Expand all". */
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

export const TreeScreen = () => {
  const treeRef = useRef<TreeListCommands>(null);
  const insets = useSafeAreaInsets();

  const tree = useMemo(() => generateFileTree(), []);
  const allFolderIds = useMemo(() => collectFolderIds(tree), [tree]);

  /* Controlled expansion; starts with the root folders open. */
  const [expandedIds, setExpandedIds] = useState<Set<string>>(
    () => new Set(tree.map((node) => node.id))
  );

  const expandAll = useCallback(
    () => setExpandedIds(new Set(allFolderIds)),
    [allFolderIds]
  );
  const collapseAll = useCallback(() => setExpandedIds(new Set()), []);

  return (
    <View style={styles.container}>
      <Tree.List
        ref={treeRef}
        data={tree}
        expandedIds={expandedIds}
        onExpandedChange={setExpandedIds}
        renderNode={({
          item,
          depth,
          indent,
          isExpanded,
          hasChildren,
          toggle,
        }) => (
          <Tree.Row
            element={item}
            depth={depth}
            indent={indent}
            isExpanded={isExpanded}
            hasChildren={hasChildren}
            onToggle={toggle}
          />
        )}
        style={styles.list}
        ListHeaderComponent={
          <ListHeader title="Files" subtitle="Virtualized directory tree" />
        }
      />
      <View
        style={[styles.toolbar, { paddingBottom: (insets.bottom || 8) + 8 }]}
      >
        <Pressable
          style={({ pressed }) => [styles.button, pressed && styles.pressed]}
          onPress={expandAll}
        >
          <Text style={styles.buttonText}>Expand All</Text>
        </Pressable>
        <Pressable
          style={({ pressed }) => [styles.button, pressed && styles.pressed]}
          onPress={collapseAll}
        >
          <Text style={styles.buttonText}>Collapse All</Text>
        </Pressable>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: colors.background,
  },
  list: {
    flex: 1,
    backgroundColor: colors.background,
  },
  toolbar: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    alignItems: 'center',
    paddingTop: 10,
    paddingHorizontal: 16,
    backgroundColor: colors.elevated,
    borderTopWidth: StyleSheet.hairlineWidth,
    borderTopColor: colors.separator,
  },
  button: {
    paddingVertical: 6,
    paddingHorizontal: 12,
  },
  pressed: {
    opacity: 0.4,
  },
  buttonText: {
    color: colors.accent,
    ...typography.body,
  },
});
