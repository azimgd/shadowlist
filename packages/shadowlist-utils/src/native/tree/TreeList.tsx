import { forwardRef, useCallback } from 'react';
import {
  TreeList as ShadowListTreeList,
  type TreeListCommands,
  type TreeListProps as ShadowListTreeListProps,
  type TreeListRenderElementInfo,
} from 'shadowlist';
import { useLabels } from '../labels';
import { defaultTreeLabels, type TreeLabels } from './labels';
import { TreeRow } from './TreeRow';
import type { TreeNode } from './types';

type BaseProps = ShadowListTreeListProps<TreeNode>;

export type TreeListProps = Omit<
  BaseProps,
  'getChildren' | 'keyExtractor' | 'renderElement'
> & {
  getChildren?: BaseProps['getChildren'];
  keyExtractor?: BaseProps['keyExtractor'];
  renderElement?: BaseProps['renderElement'];
  // Called for rows without children; rows with children toggle instead.
  onPressItem?: (item: TreeNode) => void;
  labels?: Partial<TreeLabels>;
};

const getNodeChildren = (node: TreeNode) => node.children;
const getNodeKey = (node: TreeNode) => node.id;

export const TreeList = forwardRef<TreeListCommands, TreeListProps>(
  (
    { getChildren, keyExtractor, renderElement, onPressItem, labels, ...props },
    ref
  ) => {
    const rowLabels = useLabels(defaultTreeLabels, labels);
    const renderTreeRow = useCallback(
      ({
        element,
        indent,
        isExpanded,
        hasChildren,
        toggle,
      }: TreeListRenderElementInfo<TreeNode>) => (
        <TreeRow
          item={element}
          indent={indent}
          isExpanded={isExpanded}
          hasChildren={hasChildren}
          onToggle={toggle}
          onPress={onPressItem}
          labels={rowLabels}
        />
      ),
      [onPressItem, rowLabels]
    );

    return (
      <ShadowListTreeList<TreeNode>
        ref={ref}
        getChildren={getChildren ?? getNodeChildren}
        keyExtractor={keyExtractor ?? getNodeKey}
        renderElement={renderElement ?? renderTreeRow}
        {...props}
      />
    );
  }
);
