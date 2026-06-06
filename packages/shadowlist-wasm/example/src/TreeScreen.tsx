import {
  useRef,
  useState,
  useMemo,
  useCallback,
  type CSSProperties,
} from 'react';
import { TreeList, type TreeListCommands } from 'shadowlist-wasm';
import { HeaderListItem } from './HeaderListItem';
import { TreeElement, type TreeFileNode } from './TreeElement';
import { generateFileTree } from './constants';

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

const getChildren = (node: TreeFileNode) => node.children;
const keyExtractor = (node: TreeFileNode) => node.id;

export const TreeScreen = () => {
  const treeRef = useRef<TreeListCommands>(null);

  const tree = useMemo(() => generateFileTree(), []);
  const allFolderIds = useMemo(() => collectFolderIds(tree), [tree]);

  // Controlled expansion state; starts with the root folders open.
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
    <div style={styles.container}>
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
      <div style={styles.bar}>
        <button type="button" style={styles.button} onClick={expandAll}>
          Expand all
        </button>
        <button type="button" style={styles.button} onClick={collapseAll}>
          Collapse all
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
    background: '#000000',
  },
  list: {
    flex: 1,
    minHeight: 0,
    background: '#000000',
  },
  bar: {
    display: 'flex',
    flexDirection: 'row',
    justifyContent: 'space-around',
    padding: '12px 16px',
    background: '#111111',
    borderTop: '1px solid #2F3336',
    flexShrink: 0,
  },
  button: {
    appearance: 'none',
    border: 'none',
    padding: '8px 20px',
    borderRadius: 8,
    background: '#FF9500',
    color: '#000000',
    fontWeight: 700,
    fontSize: 14,
    cursor: 'pointer',
    fontFamily: 'inherit',
  },
};
