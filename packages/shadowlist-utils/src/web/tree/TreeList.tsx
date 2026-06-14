import { forwardRef } from 'react';
import {
  TreeList as ShadowlistTreeList,
  type TreeListProps,
  type TreeListCommands,
} from 'shadowlist-wasm';
import type { TreeFileNode } from 'shadowlist-utils';
import { TreeRow } from './TreeRow';

export type TreeListProps_ = Omit<
  TreeListProps<TreeFileNode>,
  'getChildren' | 'keyExtractor' | 'renderElement'
> & {
  getChildren?: TreeListProps<TreeFileNode>['getChildren'];
  keyExtractor?: TreeListProps<TreeFileNode>['keyExtractor'];
  renderElement?: TreeListProps<TreeFileNode>['renderElement'];
};

const defaultGetChildren: TreeListProps<TreeFileNode>['getChildren'] = (node) =>
  node.children;
const defaultKeyExtractor: TreeListProps<TreeFileNode>['keyExtractor'] = (
  node
) => node.id;
const defaultRenderElement: TreeListProps<TreeFileNode>['renderElement'] = ({
  element,
  depth,
  indent,
  isExpanded,
  hasChildren,
  toggle,
}) => (
  <TreeRow
    element={element}
    depth={depth}
    indent={indent}
    isExpanded={isExpanded}
    hasChildren={hasChildren}
    onToggle={toggle}
  />
);

// A virtualized, expandable directory tree over `TreeFileNode` data.
export const TreeList = forwardRef<TreeListCommands, TreeListProps_>(
  ({ getChildren, keyExtractor, renderElement, ...props }, ref) => (
    <ShadowlistTreeList<TreeFileNode>
      ref={ref}
      getChildren={getChildren ?? defaultGetChildren}
      keyExtractor={keyExtractor ?? defaultKeyExtractor}
      renderElement={renderElement ?? defaultRenderElement}
      {...props}
    />
  )
);
