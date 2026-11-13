/*
 * Relative time tests: which unit a past moment is told in.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/utils/RelativeTime.hpp>

#include <cstdint>
#include <limits>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

constexpr double NOW_MS = 1762905600000.0;

RelativeTime ago(double elapsedMs) {
  return relativeTime(NOW_MS - elapsedMs, NOW_MS);
}

}

TEST(a_time_under_a_minute_is_now) {
  CHECK(ago(0.0).unit == RelativeTimeUnit::Now);
  CHECK(ago(RELATIVE_TIME_MINUTE_MS - 1.0).unit == RelativeTimeUnit::Now);
  CHECK_EQ(ago(30000.0).count, std::uint32_t(0));
}

TEST(a_date_ahead_of_now_is_now) {
  CHECK(ago(-RELATIVE_TIME_DAY_MS).unit == RelativeTimeUnit::Now);
}

TEST(a_time_under_an_hour_counts_whole_minutes) {
  CHECK(ago(RELATIVE_TIME_MINUTE_MS).unit == RelativeTimeUnit::Minutes);
  CHECK_EQ(ago(RELATIVE_TIME_MINUTE_MS).count, std::uint32_t(1));
  CHECK_EQ(ago(2.0 * RELATIVE_TIME_MINUTE_MS - 1.0).count, std::uint32_t(1));
  CHECK_EQ(ago(RELATIVE_TIME_HOUR_MS - 1.0).count, std::uint32_t(59));
}

TEST(a_time_under_a_day_counts_whole_hours) {
  CHECK(ago(RELATIVE_TIME_HOUR_MS).unit == RelativeTimeUnit::Hours);
  CHECK_EQ(ago(RELATIVE_TIME_HOUR_MS).count, std::uint32_t(1));
  CHECK_EQ(ago(RELATIVE_TIME_DAY_MS - 1.0).count, std::uint32_t(23));
}

TEST(a_time_under_seven_days_counts_whole_days) {
  CHECK(ago(RELATIVE_TIME_DAY_MS).unit == RelativeTimeUnit::Days);
  CHECK_EQ(ago(RELATIVE_TIME_DAY_MS).count, std::uint32_t(1));
  CHECK_EQ(ago(RELATIVE_TIME_WEEK_MS - 1.0).count, std::uint32_t(6));
}

TEST(a_time_seven_days_old_is_a_date) {
  CHECK(ago(RELATIVE_TIME_WEEK_MS).unit == RelativeTimeUnit::Date);
  CHECK_EQ(ago(RELATIVE_TIME_WEEK_MS).count, std::uint32_t(0));
  CHECK(ago(400.0 * RELATIVE_TIME_DAY_MS).unit == RelativeTimeUnit::Date);
}

TEST(a_time_that_is_not_a_number_is_a_date) {
  double nan = std::numeric_limits<double>::quiet_NaN();
  double infinity = std::numeric_limits<double>::infinity();
  CHECK(relativeTime(nan, NOW_MS).unit == RelativeTimeUnit::Date);
  CHECK(relativeTime(NOW_MS, nan).unit == RelativeTimeUnit::Date);
  CHECK(relativeTime(-infinity, NOW_MS).unit == RelativeTimeUnit::Date);
  CHECK(relativeTime(infinity, NOW_MS).unit == RelativeTimeUnit::Now);
}

TEST(a_time_that_is_not_finite_has_no_date) {
  CHECK(relativeTimeHasDate(0.0));
  CHECK(relativeTimeHasDate(-86400000.0));
  CHECK(relativeTimeHasDate(1794441600000.0));
  CHECK(!relativeTimeHasDate(std::numeric_limits<double>::quiet_NaN()));
  CHECK(!relativeTimeHasDate(std::numeric_limits<double>::infinity()));
  CHECK(!relativeTimeHasDate(-std::numeric_limits<double>::infinity()));
}
