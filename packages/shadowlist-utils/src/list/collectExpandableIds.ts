export interface CollectExpandableIdsOptions<NodeT> {
  getChildren: (node: NodeT) => ReadonlyArray<NodeT> | undefined;
  keyExtractor: (node: NodeT) => string;
}

/**
 * The id of every node with at least one child, at any depth. Use it as `expandedIds` for
 * an expand all action on a TreeList. It uses a loop, so deep trees can't overflow the stack.
 *
 * @example
 * setExpandedIds(new Set(collectExpandableIds(tree, { getChildren, keyExtractor })));
 */
export function collectExpandableIds<NodeT>(
  nodes: ReadonlyArray<NodeT>,
  { getChildren, keyExtractor }: CollectExpandableIdsOptions<NodeT>
): string[] {
  const ids: string[] = [];
  const stack = [...nodes];
  while (stack.length > 0) {
    const node = stack.pop()!;
    const children = getChildren(node);
    if (children && children.length > 0) {
      ids.push(keyExtractor(node));
      stack.push(...children);
    }
  }
  return ids;
}
