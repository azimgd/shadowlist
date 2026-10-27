export interface BoardLabels {
  emptyColumn: string;
  emptyColumnReadOnly: string;
  columnSummary: (title: string, count: number) => string;
}

export const defaultBoardLabels: BoardLabels = {
  emptyColumn: 'Drop cards here',
  emptyColumnReadOnly: 'No cards',
  columnSummary: (title, count) =>
    `${title}, ${count} ${count === 1 ? 'card' : 'cards'}`,
};
