/*
 * Poll tests: the votes in all, the leading option and an option's share.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/utils/PollTally.hpp>

#include <cstddef>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(a_poll_tally_sums_the_votes) {
  CHECK_EQ(pollTally({3.0, 9.0, 1.0}).totalVotes, 13.0);
  CHECK_EQ(pollTally({}).totalVotes, 0.0);
}

TEST(the_option_with_the_most_votes_leads) {
  CHECK_EQ(pollTally({3.0, 9.0, 1.0}).leadingIndex, std::size_t(1));
  CHECK_EQ(pollTally({1.0, 2.0, 3.0}).leadingIndex, std::size_t(2));
}

TEST(the_first_of_two_equal_options_leads) {
  CHECK_EQ(pollTally({5.0, 5.0, 2.0}).leadingIndex, std::size_t(0));
  CHECK_EQ(pollTally({0.0, 4.0, 4.0}).leadingIndex, std::size_t(1));
}

TEST(the_first_option_leads_a_poll_nobody_voted_in) {
  CHECK_EQ(pollTally({0.0, 0.0}).leadingIndex, std::size_t(0));
}

TEST(a_poll_without_options_has_no_leader) {
  CHECK_EQ(pollTally({}).leadingIndex, UNDEFINED_INDEX);
}

TEST(a_poll_percent_rounds_to_a_whole_number_with_a_half_going_up) {
  CHECK_EQ(pollPercent(1.0, 3.0), 33);
  CHECK_EQ(pollPercent(2.0, 3.0), 67);
  CHECK_EQ(pollPercent(1.0, 8.0), 13);
  CHECK_EQ(pollPercent(1.0, 200.0), 1);
  CHECK_EQ(pollPercent(5.0, 5.0), 100);
  CHECK_EQ(pollPercent(0.0, 5.0), 0);
}

TEST(a_poll_percent_without_votes_is_zero) {
  CHECK_EQ(pollPercent(0.0, 0.0), 0);
  CHECK_EQ(pollPercent(3.0, 0.0), 0);
  CHECK_EQ(pollPercent(3.0, -2.0), 0);
}
