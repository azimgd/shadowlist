export interface ReorderLabels {
  dragHint: string;
  moveUp: string;
  moveDown: string;
}

export const defaultReorderLabels: ReorderLabels = {
  dragHint: 'Press and hold, then drag to reorder',
  moveUp: 'Move up',
  moveDown: 'Move down',
};
