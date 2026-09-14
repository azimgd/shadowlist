#pragma once

#include <cstddef>
#include <cstdint>
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

  // Gap after the element before the next one.
  double gapX = 0.0;
  double gapY = 0.0;

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
   * matters in two places: a predicted size must not feed the frozen average (which
   * describes real measurements only), and a predicted row may be dematerialized before it
   * has ever been mounted, which the `measured` guard alone would forbid.
   */
  bool predicted = false;

  /*
   * Stable debug id, 16 hex digits. Operational identity is `key`; this exists purely
   * for log/JSON correlation (see Revision::getDebugRepresentation), so it is generated
   * on first use rather than in the constructor: a 100k-row list would otherwise pay a
   * random draw and a stream format per row before showing anything. The seed is drawn
   * once per element from a thread-local counter mixed with a process-unique salt, so
   * ids stay unique and stable for an element's lifetime without a per-element RNG draw.
   */
  const std::string& getId() const {
    if (this->debugId.empty()) {
      this->debugId = formatHex(nextIdSeed());
    }
    return this->debugId;
  }

private:
  mutable std::string debugId = "";

  /*
   * SplitMix64 over a thread-local counter salted with the container's address space.
   * Cheap (no allocation, no locking, no <random> machinery) and collision-free within
   * a thread, which is all a debug id needs.
   */
  static std::uint64_t nextIdSeed() {
    thread_local std::uint64_t counter = reinterpret_cast<std::uint64_t>(&counter) * 0x9E3779B97F4A7C15ull;
    counter += 0x9E3779B97F4A7C15ull;
    std::uint64_t mixed = counter;
    mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9ull;
    mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EBull;
    return mixed ^ (mixed >> 31);
  }

  static std::string formatHex(std::uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int position = 15; position >= 0; --position) {
      out[static_cast<std::size_t>(position)] = digits[value & 0xF];
      value >>= 4;
    }
    return out;
  }
};

}
