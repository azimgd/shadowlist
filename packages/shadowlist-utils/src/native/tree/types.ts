export type TreeNodeKind = 'folder' | 'file';

export interface TreeNode {
  id: string;
  name: string;
  children?: ReadonlyArray<TreeNode>;
  // Defaults to 'folder' when `children` is set, otherwise 'file'.
  kind?: TreeNodeKind;
}
