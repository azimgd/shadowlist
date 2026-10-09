export interface PollLabels {
  totalVotes: (count: number) => string;
  option: (label: string, percent: number) => string;
}

export const defaultPollLabels: PollLabels = {
  totalVotes: (count) => (count === 1 ? '1 vote' : `${count} votes`),
  option: (label, percent) => `${label}, ${percent}%`,
};
