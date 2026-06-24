#pragma once

#include <string>
#include <random>
#include <sstream>
#include <iomanip>

namespace azimgd::shadowlist {

struct Size {
  double width;
  double height;
};

class Element {
public:
  Element() : id(generateRandomId()) {}

  // Random id, generated once per element instance.
  std::string id = "";

  // User-provided key, used to match elements across data updates.
  std::string key = "";

  // Position of this element in the list.
  std::size_t index = 0;

  double width = 0.0;
  double height = 0.0;

  // Top-left position within the container.
  double offsetX = 0.0;
  double offsetY = 0.0;

  // Gap after the element before the next one.
  double gapX = 0.0;
  double gapY = 0.0;

  // Set once a size estimate has been applied.
  bool estimated = false;

  // Set once the element has been natively measured.
  bool measured = false;

  // Generate a random 16-hex-digit id.
  static std::string generateRandomId() {
    thread_local std::mt19937_64 gen(std::random_device{}());
    thread_local std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << dis(gen);
    return ss.str();
  }
};

}
