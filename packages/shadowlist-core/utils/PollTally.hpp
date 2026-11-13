#pragma once

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/utils/Rounding.hpp>

#include <cmath>
#include <cstddef>
#include <vector>

namespace azimgd::shadowlist {

/*
 * A poll's votes in all and its leading option, UNDEFINED_INDEX for a poll without options.
 */
struct PollTally {
  double totalVotes = 0.0;
  std::size_t leadingIndex = UNDEFINED_INDEX;
};

/*
 * The votes in all and the first option with strictly the most. The first option leads a poll
 * nobody voted in yet.
 */
inline PollTally pollTally(const std::vector<double>& votes) {
  PollTally tally;
  for (std::size_t index = 0; index < votes.size(); ++index) {
    tally.totalVotes += votes[index];
    if (tally.leadingIndex == UNDEFINED_INDEX || votes[index] > votes[tally.leadingIndex]) {
      tally.leadingIndex = index;
    }
  }
  return tally;
}

/*
 * An option's share in whole percent, a half going up. 0 without votes.
 */
inline int pollPercent(double votes, double totalVotes) {
  if (!(totalVotes > 0.0)) {
    return 0;
  }
  double percent = roundHalfUp(votes / totalVotes * 100.0);
  return std::isfinite(percent) ? static_cast<int>(percent) : 0;
}

}
