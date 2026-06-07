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
  'getChildren' | 'keyExtractor' | 'renderNode'
> & {
  getChildren?: TreeListProps<TreeFileNode>['getChildren'];
  keyExtractor?: TreeListProps<TreeFileNode>['keyExtractor'];
  renderNode?: TreeListProps<TreeFileNode>['renderNode'];
};

const defaultGetChildren: TreeListProps<TreeFileNode>['getChildren'] = (node) =>
  node.children;
const defaultKeyExtractor: TreeListProps<TreeFileNode>['keyExtractor'] = (node) =>
  node.id;
const defaultRenderNode: TreeListProps<TreeFileNode>['renderNode'] = ({
  item,
  depth,
  indent,
  isExpanded,
  hasChildren,
  toggle,
}) => (
  <TreeRow
    element={item}
    depth={depth}
    indent={indent}
    isExpanded={isExpanded}
    hasChildren={hasChildren}
    onToggle={toggle}
  />
);

// A virtualized, expandable directory tree over `TreeFileNode` data.
export const TreeList = forwardRef<TreeListCommands, TreeListProps_>(
  ({ getChildren, keyExtractor, renderNode, ...props }, ref) => (
    <ShadowlistTreeList<TreeFileNode>
      ref={ref}
      getChildren={getChildren ?? defaultGetChildren}
      keyExtractor={keyExtractor ?? defaultKeyExtractor}
      renderNode={renderNode ?? defaultRenderNode}
      {...props}
    />
  )
);
