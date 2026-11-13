#include "TestFramework.hpp"
#include "VectorHelpers.hpp"

#include <shadowlist-core/utils/Progress.hpp>

#include <cmath>
#include <limits>

using namespace azimgd::shadowlist;
using namespace slt;

TEST(progress_fraction_clamps_finite_values_and_rejects_nonfinite_values) {
  CHECK_EQ(clampFraction(-2.0), 0.0);
  CHECK_EQ(clampFraction(0.125), 0.125);
  CHECK_EQ(clampFraction(4.0), 1.0);
  CHECK_EQ(clampFraction(std::numeric_limits<double>::quiet_NaN()), 0.0);
  CHECK_EQ(clampFraction(std::numeric_limits<double>::infinity()), 0.0);
  CHECK_EQ(clampFraction(-std::numeric_limits<double>::infinity()), 0.0);
  CHECK(!std::signbit(clampFraction(-0.0)));
}

TEST(progress_accessibility_percent_uses_the_RN_positive_halfway_rule) {
  CHECK_EQ(progressPercent(0.0049), 0);
  CHECK_EQ(progressPercent(0.005), 1);
  CHECK_EQ(progressPercent(0.1249), 12);
  CHECK_EQ(progressPercent(0.125), 13);
  CHECK_EQ(progressPercent(0.285), 28);
  CHECK_EQ(progressPercent(0.295), 30);
  CHECK_EQ(progressPercent(0.5), 50);
  CHECK_EQ(progressPercent(0.995), 100);
  CHECK_EQ(progressPercent(2.0), 100);
  CHECK_EQ(progressPercent(std::numeric_limits<double>::quiet_NaN()), 0);
}

VECTOR_TEST(progress_vectors_replay, "progress.json") {
  double fraction = vector["input"]["fraction"].asOptionalNumber();
  CHECK_EQ(clampFraction(fraction), vector["output"]["fraction"].number);
  CHECK_EQ(progressPercent(fraction), static_cast<int>(vector["output"]["percent"].number));
}
