export interface CollectExpandableKeysOptions<NodeT> {
  getChildren: (node: NodeT) => ReadonlyArray<NodeT> | undefined;
  keyExtractor: (node: NodeT) => string;
}

/*
 * The key of every node with at least one child, at any depth. Use it as `expandedKeys` for
 * an expand all action on a TreeList. It uses a loop. Deep trees can't overflow the stack.
 *
 * For example:
 *
 *   setExpandedKeys(new Set(collectExpandableKeys(tree, { getChildren, keyExtractor })));
 */
export function collectExpandableKeys<NodeT>(
  nodes: ReadonlyArray<NodeT>,
  { getChildren, keyExtractor }: CollectExpandableKeysOptions<NodeT>
): string[] {
  const keys: string[] = [];
  const stack = [...nodes];
  while (stack.length > 0) {
    const node = stack.pop()!;
    const children = getChildren(node);
    if (children && children.length > 0) {
      keys.push(keyExtractor(node));
      stack.push(...children);
    }
  }
  return keys;
}
