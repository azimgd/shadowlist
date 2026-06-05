import { memo, type CSSProperties } from 'react';

/*
 * A file-system node for the TreeList example. Children present => folder; the tree
 * is described by this nested shape, but TreeList itself never requires it - it
 * reaches children through the getChildren accessor the screen passes in.
 */
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

/*
 * One row of the directory browser. Purely presentational: indentation comes from
 * `indent` (depth * indentWidth, resolved by TreeList), the disclosure chevron and
 * folder/file glyph reflect the node, and a click toggles expansion. memo'd so that
 * while scrolling - when TreeList hands back the same row identity - the row never
 * re-renders.
 */
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
