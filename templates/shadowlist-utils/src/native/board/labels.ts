export interface BoardLabels {
  emptyColumn: string;
  columnSummary: (title: string, count: number) => string;
}

export const defaultBoardLabels: BoardLabels = {
  emptyColumn: 'Drop cards here',
  columnSummary: (title, count) =>
    `${title}, ${count} ${count === 1 ? 'card' : 'cards'}`,
};
