export interface CollectExpandableIdsOptions<NodeT> {
  getChildren: (node: NodeT) => ReadonlyArray<NodeT> | undefined;
  keyExtractor: (node: NodeT) => string;
}

/**
 * The id of every node that has at least one child, at any depth: the `expandedIds` for
 * an "Expand all" action on a TreeList. Iterative, so deep trees cannot overflow the stack.
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
