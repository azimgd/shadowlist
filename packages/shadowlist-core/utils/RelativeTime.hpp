#pragma once

#include <cmath>
#include <cstdint>

namespace azimgd::shadowlist {

/*
 * A minute, an hour, a day and seven days, in milliseconds.
 */
constexpr double RELATIVE_TIME_MINUTE_MS = 60000.0;
constexpr double RELATIVE_TIME_HOUR_MS = 60.0 * RELATIVE_TIME_MINUTE_MS;
constexpr double RELATIVE_TIME_DAY_MS = 24.0 * RELATIVE_TIME_HOUR_MS;
constexpr double RELATIVE_TIME_WEEK_MS = 7.0 * RELATIVE_TIME_DAY_MS;

enum class RelativeTimeUnit { Now, Minutes, Hours, Days, Date };

/*
 * How long ago a moment was. count is the whole minutes, hours or days, and 0 for Now and Date.
 */
struct RelativeTime {
  RelativeTimeUnit unit = RelativeTimeUnit::Now;
  std::uint32_t count = 0;
};

/*
 * How long ago dateMs was at nowMs: Now under a minute and for a date ahead of now, Minutes
 * under an hour, Hours under a day, Days under seven days, and Date after that, which the host
 * writes as a calendar date. A time that is not a number is a Date too.
 */
inline RelativeTime relativeTime(double dateMs, double nowMs) {
  double elapsedMs = nowMs - dateMs;
  if (std::isnan(elapsedMs)) {
    return {RelativeTimeUnit::Date, 0};
  }
  if (elapsedMs < RELATIVE_TIME_MINUTE_MS) {
    return {RelativeTimeUnit::Now, 0};
  }
  if (elapsedMs < RELATIVE_TIME_HOUR_MS) {
    return {RelativeTimeUnit::Minutes, static_cast<std::uint32_t>(elapsedMs / RELATIVE_TIME_MINUTE_MS)};
  }
  if (elapsedMs < RELATIVE_TIME_DAY_MS) {
    return {RelativeTimeUnit::Hours, static_cast<std::uint32_t>(elapsedMs / RELATIVE_TIME_HOUR_MS)};
  }
  if (elapsedMs < RELATIVE_TIME_WEEK_MS) {
    return {RelativeTimeUnit::Days, static_cast<std::uint32_t>(elapsedMs / RELATIVE_TIME_DAY_MS)};
  }
  return {RelativeTimeUnit::Date, 0};
}

/*
 * Whether dateMs is a moment a host can write as a calendar date, which a time that is not a
 * number or is infinite is not. The host shows no text for such a time.
 */
inline bool relativeTimeHasDate(double dateMs) {
  return std::isfinite(dateMs);
}

}
