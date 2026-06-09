/*
 * Flattens the visible subtree (nodes whose ancestors are all expanded) into one
 * element stream via iterative pre-order DFS; collapsed subtrees are never descended
 * into. Each row keeps a stable node-id key so surviving rows reconcile across an
 * expand/collapse toggle. Also builds id -> flat index for scrollToNode.
 */

export interface TreeFlatRow<ItemT> {
  /* Stable key for the flattened row; equals keyExtractor(item). */
  id: string;
  item: ItemT;
  depth: number;
  hasChildren: boolean;
  isExpanded: boolean;
}

export interface FlattenTreeResult<ItemT> {
  rows: TreeFlatRow<ItemT>[];
  indexByKey: Map<string, number>;
}

export const flattenTree = <ItemT>(
  data: ReadonlyArray<ItemT>,
  getChildren: (item: ItemT) => ReadonlyArray<ItemT> | undefined,
  keyExtractor: (item: ItemT) => string,
  expandedSet: ReadonlySet<string>
): FlattenTreeResult<ItemT> => {
  const rows: TreeFlatRow<ItemT>[] = [];
  const indexByKey = new Map<string, number>();

  interface Frame {
    item: ItemT;
    depth: number;
  }
  const stack: Frame[] = [];
  for (let index = data.length - 1; index >= 0; index--) {
    stack.push({ item: data[index] as ItemT, depth: 0 });
  }

  while (stack.length > 0) {
    const { item, depth } = stack.pop() as Frame;
    const id = keyExtractor(item);
    const children = getChildren(item);
    const hasChildren = !!children && children.length > 0;
    const isExpanded = hasChildren && expandedSet.has(id);

    indexByKey.set(id, rows.length);
    rows.push({ id, item, depth, hasChildren, isExpanded });

    if (isExpanded && children) {
      for (let index = children.length - 1; index >= 0; index--) {
        stack.push({ item: children[index] as ItemT, depth: depth + 1 });
      }
    }
  }

  return { rows, indexByKey };
};
