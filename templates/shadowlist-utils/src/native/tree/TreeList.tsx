import { forwardRef, useCallback } from 'react';
import {
  TreeList as ShadowListTreeList,
  type TreeListCommands,
  type TreeListProps as ShadowListTreeListProps,
  type TreeListRenderItemInfo,
} from 'shadowlist';
import { useLabels } from '../labels';
import { defaultTreeLabels, type TreeLabels } from './labels';
import { TreeRow } from './TreeRow';
import type { TreeNode } from './types';

type BaseProps = ShadowListTreeListProps<TreeNode>;

export type TreeListProps = Omit<
  BaseProps,
  'getChildren' | 'keyExtractor' | 'renderItem'
> & {
  getChildren?: BaseProps['getChildren'];
  keyExtractor?: BaseProps['keyExtractor'];
  renderItem?: BaseProps['renderItem'];
  onPressItem?: (item: TreeNode) => void;
  labels?: Partial<TreeLabels>;
};

const getNodeChildren = (node: TreeNode) => node.children;
const getNodeKey = (node: TreeNode) => node.id;

export const TreeList = forwardRef<TreeListCommands, TreeListProps>(
  (
    { getChildren, keyExtractor, renderItem, onPressItem, labels, ...props },
    ref
  ) => {
    const rowLabels = useLabels(defaultTreeLabels, labels);
    const renderTreeRow = useCallback(
      ({
        item,
        indent,
        isExpanded,
        hasChildren,
        toggle,
      }: TreeListRenderItemInfo<TreeNode>) => (
        <TreeRow
          item={item}
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
        renderItem={renderItem ?? renderTreeRow}
        {...props}
      />
    );
  }
);
