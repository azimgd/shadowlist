import { memo, type CSSProperties } from 'react';

// A file-system node for the TreeList example. Nodes with children are folders.
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

// One presentational row of the directory browser; a click toggles expansion.
export const TreeElement = memo(
  ({ element, indent, isExpanded, hasChildren, onToggle }: TreeElementProps) => {
    const isFolder = element.type === 'folder';

    return (
      <div
        style={{ ...styles.row, cursor: hasChildren ? 'pointer' : 'default' }}
        onClick={hasChildren ? onToggle : undefined}
      >
        <div style={{ ...styles.indent, width: indent }} />
        <span style={styles.chevron}>
          {hasChildren ? (isExpanded ? '▾' : '▸') : ' '}
        </span>
        <span style={styles.glyph}>{isFolder ? '📁' : '📄'}</span>
        <span style={styles.name}>{element.name}</span>
        {isFolder && element.children ? (
          <span style={styles.count}>{element.children.length}</span>
        ) : null}
      </div>
    );
  }
);

const styles: Record<string, CSSProperties> = {
  row: {
    display: 'flex',
    alignItems: 'center',
    height: 40,
    paddingRight: 16,
    background: '#000000',
    boxSizing: 'border-box',
  },
  indent: {
    height: '100%',
    flexShrink: 0,
  },
  chevron: {
    width: 18,
    textAlign: 'center',
    color: '#FF9500',
    fontSize: 14,
    flexShrink: 0,
  },
  glyph: {
    fontSize: 16,
    marginRight: 8,
    flexShrink: 0,
  },
  name: {
    flex: 1,
    color: '#FFFFFF',
    fontSize: 15,
    overflow: 'hidden',
    textOverflow: 'ellipsis',
    whiteSpace: 'nowrap',
  },
  count: {
    color: '#666666',
    fontSize: 13,
    marginLeft: 8,
    flexShrink: 0,
  },
};
