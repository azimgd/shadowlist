import { memo, type CSSProperties } from 'react';
import { colors, typography } from './theme';
import { Chevron, Folder, Doc } from './icons';

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
          {hasChildren ? (
            <Chevron
              direction={isExpanded ? 'down' : 'right'}
              color={colors.tertiaryLabel}
              size={14}
              strokeWidth={1.75}
            />
          ) : null}
        </span>
        <span style={styles.glyph}>
          {isFolder ? <Folder size={20} /> : <Doc size={18} />}
        </span>
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
    height: 44,
    paddingRight: 16,
    background: colors.background,
    boxSizing: 'border-box',
  },
  indent: {
    height: '100%',
    flexShrink: 0,
  },
  chevron: {
    width: 22,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    flexShrink: 0,
  },
  glyph: {
    width: 24,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    marginRight: 8,
    flexShrink: 0,
  },
  name: {
    flex: 1,
    color: colors.label,
    ...typography.callout,
    overflow: 'hidden',
    textOverflow: 'ellipsis',
    whiteSpace: 'nowrap',
  },
  count: {
    color: colors.tertiaryLabel,
    ...typography.footnote,
    marginLeft: 8,
    flexShrink: 0,
  },
};
