#pragma once

#include <shadowlist-core/utils/Rounding.hpp>

#include <algorithm>
#include <cmath>

namespace azimgd::shadowlist {

/* The RN ProgressBar/ProgressRing contract: nonfinite values mean no progress. */
inline double clampFraction(double fraction) {
  return std::max(0.0, std::min(1.0, std::isfinite(fraction) ? fraction : 0.0));
}

/* Whole accessibility percent, with the same positive halfway rounding as Math.round. */
inline int progressPercent(double fraction) {
  return static_cast<int>(roundHalfUp(clampFraction(fraction) * 100.0));
}

}
