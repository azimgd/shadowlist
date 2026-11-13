#pragma once

#include <cmath>

namespace azimgd::shadowlist {

/*
 * value rounded to the nearest whole number, a half going up. It is what JS Math.round gives,
 * where std::round takes a negative half down.
 */
inline double roundHalfUp(double value) {
  double floor = std::floor(value);
  return value - floor >= 0.5 ? floor + 1.0 : floor;
}

}
