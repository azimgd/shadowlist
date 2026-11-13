import type { PollOption } from './types';

export interface PollTally {
  totalVotes: number;
  leadingId: string | undefined;
}

/*
 * The votes in all and the first option with strictly the most. pollTally and pollPercent in
 * the core's utils/PollTally.hpp are the same rules, and vectors.test holds them equal.
 */
export function getPollTally(options: ReadonlyArray<PollOption>): PollTally {
  let total = 0;
  let leader: PollOption | undefined;
  for (const option of options) {
    total += option.votes;
    if (leader === undefined || option.votes > leader.votes) {
      leader = option;
    }
  }
  return { totalVotes: total, leadingId: leader?.id };
}

/*
 * An option's share in whole percent, 0 without votes.
 */
export function getPollPercent(votes: number, totalVotes: number): number {
  return totalVotes > 0 ? Math.round((votes / totalVotes) * 100) : 0;
}
