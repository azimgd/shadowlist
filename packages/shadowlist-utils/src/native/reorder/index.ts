import { ReorderList } from './ReorderList';
import { ReorderRow } from './ReorderRow';

export type { ReorderListProps } from './ReorderList';
export type { ReorderRowProps } from './ReorderRow';

/* Reorder domain: <Reorder.List data={...} onReorder={...} /> + <Reorder.Row />. */
export const Reorder = {
  List: ReorderList,
  Row: ReorderRow,
};
