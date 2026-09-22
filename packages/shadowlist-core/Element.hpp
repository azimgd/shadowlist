#pragma once

#include <cstddef>
#include <string>

namespace azimgd::shadowlist {

struct Size {
  double width;
  double height;
};

class Element {
public:
  // The user's key, used to match rows across data updates.
  std::string key = "";

  std::size_t index = 0;

  double width = 0.0;
  double height = 0.0;

  // Top left corner inside the container.
  double offsetX = 0.0;
  double offsetY = 0.0;

  // Set once the row has an estimated size.
  bool estimated = false;

  // Set once the row has been measured natively.
  bool measured = false;

  /*
   * Set when the size came from a host measurement made ahead of layout, not from the fallback estimate.
   * Such a row is also estimated but not measured, and its size must never feed the average,
   * which only counts real measurements.
   */
  bool predicted = false;
};

}
