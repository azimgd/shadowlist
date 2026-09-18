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
  // User-provided key, used to match elements across data updates.
  std::string key = "";

  // Position of this element in the list.
  std::size_t index = 0;

  double width = 0.0;
  double height = 0.0;

  // Top-left position within the container.
  double offsetX = 0.0;
  double offsetY = 0.0;

  // Set once a size estimate has been applied.
  bool estimated = false;

  // Set once the element has been natively measured.
  bool measured = false;

  /*
   * Set when this element's size came from an ahead-of-time host measurement
   * (see Container::predictedSizes) rather than from the generic fallback estimate.
   *
   * A predicted row carries `estimated = true` so the fallback passes leave it alone, but
   * `measured` stays false because nothing has laid it out natively yet. The distinction
   * matters because a predicted size must not feed the frozen average (which describes real
   * measurements only).
   */
  bool predicted = false;
};

}
