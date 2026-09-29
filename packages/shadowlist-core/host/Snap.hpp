#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

namespace azimgd::shadowlist {

/*
 * The snap offset nearest to target, or target when there are none. The first of two equally
 * near offsets wins. With roundOffsets each offset is rounded to a whole number first, which
 * is how Android compares offsets in pixels.
 */
inline double nearestSnapOffset(const double* offsets, std::size_t count, double target, bool roundOffsets = false) {
  if (count == 0) {
    return target;
  }
  auto candidate = [roundOffsets](double offset) {
    return roundOffsets ? std::floor(offset + 0.5) : offset;
  };
  double best = candidate(offsets[0]);
  double bestDistance = std::fabs(best - target);
  for (std::size_t index = 1; index < count; ++index) {
    double offset = candidate(offsets[index]);
    double distance = std::fabs(offset - target);
    if (distance < bestDistance) {
      bestDistance = distance;
      best = offset;
    }
  }
  return best;
}

inline double nearestSnapOffset(const std::vector<double>& offsets, double target, bool roundOffsets = false) {
  return nearestSnapOffset(offsets.data(), offsets.size(), target, roundOffsets);
}

}
