import { useRef, useState, useMemo, useCallback, type CSSProperties } from 'react';
import { type TreeListCommands } from 'shadowlist-wasm';
import { Tree, ListHeader, colors, typography } from 'shadowlist-utils/web';
import { generateFileTree, type TreeFileNode } from 'shadowlist-utils';

// Collect every folder id in the tree. Used by "Expand all".
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

  const tree = useMemo(() => generateFileTree(), []);
  const allFolderIds = useMemo(() => collectFolderIds(tree), [tree]);

  // Controlled expansion; starts with the root folders open.
  const [expandedIds, setExpandedIds] = useState<Set<string>>(
    () => new Set(tree.map((node) => node.id))
  );

  const expandAll = useCallback(
    () => setExpandedIds(new Set(allFolderIds)),
    [allFolderIds]
  );
  const collapseAll = useCallback(() => setExpandedIds(new Set()), []);

  return (
    <div style={styles.container}>
      <Tree.List
        ref={treeRef}
        data={tree}
        expandedIds={expandedIds}
        onExpandedChange={setExpandedIds}
        renderNode={({ item, depth, indent, isExpanded, hasChildren, toggle }) => (
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
      <div style={styles.toolbar}>
        <button type="button" style={styles.button} onClick={expandAll}>
          Expand All
        </button>
        <button type="button" style={styles.button} onClick={collapseAll}>
          Collapse All
        </button>
      </div>
    </div>
  );
};

const styles: Record<string, CSSProperties> = {
  container: {
    position: 'relative',
    display: 'flex',
    flexDirection: 'column',
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
  list: {
    flex: 1,
    minHeight: 0,
    background: colors.background,
  },
  toolbar: {
    display: 'flex',
    flexDirection: 'row',
    justifyContent: 'space-around',
    alignItems: 'center',
    padding: '10px 16px',
    background: colors.elevated,
    borderTop: `1px solid ${colors.separator}`,
    flexShrink: 0,
  },
  button: {
    appearance: 'none',
    border: 'none',
    background: 'transparent',
    padding: '6px 12px',
    color: colors.accent,
    ...typography.body,
    cursor: 'pointer',
    fontFamily: 'inherit',
  },
};
