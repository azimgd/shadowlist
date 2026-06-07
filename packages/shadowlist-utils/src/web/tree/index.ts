import { TreeList } from './TreeList';
import { TreeRow } from './TreeRow';

export type { TreeListProps_ } from './TreeList';
export type { TreeRowProps } from './TreeRow';

// Tree domain: <Tree.List data={tree} expandedIds={...} /> + <Tree.Row />.
export const Tree = {
  List: TreeList,
  Row: TreeRow,
};
