import type { TreeNode } from 'shadowlist-utils/native';
import { generateFileTree } from '../fixtures/tree';
import { request, type RequestSignal } from './network';

let tree: TreeNode[] | undefined;

export function fetchFileTree(signal?: RequestSignal): Promise<TreeNode[]> {
  return request(() => {
    if (!tree) tree = generateFileTree();
    return tree;
  }, signal);
}
