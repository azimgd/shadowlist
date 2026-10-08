export type TreeNodeKind = 'folder' | 'file';

export interface TreeNode {
  id: string;
  name: string;
  children?: ReadonlyArray<TreeNode>;
  kind?: TreeNodeKind;
}
