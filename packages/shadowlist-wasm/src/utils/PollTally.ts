/*
 * leadingIndex is -1 for a poll without options. The first option wins a tie.
 */
export interface PollTally {
  totalVotes: number;
  leadingIndex: number;
}
