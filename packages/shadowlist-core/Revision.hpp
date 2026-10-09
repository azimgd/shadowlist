#pragma once

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Element.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace azimgd::shadowlist {

class Revision final {
public:
  std::vector<Element> elements;

  /*
   * Maps each key to its row index so anchors resolve without a scan. The first duplicate wins.
   * Stored values include indexBias. Always use indexForKey and setIndexForKey. Reading
   * a value directly gives a wrong index, and rows end up taking other rows' sizes.
   */
  std::unordered_map<std::string, std::size_t> elementIndexByKey;

  /*
   * Added to every stored index. A prepend only changes this number instead of every entry.
   * Unsigned wraparound is fine because the bias and the values wrap together.
   * A full rebuild sets it back to zero.
   */
  std::size_t indexBias = 0;

  /*
   * Whether two rows share a key. The map then holds only the first, and removing rows
   * needs the full rebuild to find which copy takes over.
   */
  bool hasDuplicateKeys = false;

  /*
   * Look up a key's row index, or UNDEFINED_INDEX when the key is missing.
   */
  std::size_t indexForKey(const std::string& key) const {
    auto entry = elementIndexByKey.find(key);
    if (entry == elementIndexByKey.end()) {
      return UNDEFINED_INDEX;
    }
    return entry->second - indexBias;
  }

  /*
   * Store the key's index unless the key is already there. Returns true if it was new.
   */
  bool setIndexForKey(const std::string& key, std::size_t index) {
    return elementIndexByKey.emplace(key, index + indexBias).second;
  }

  double containerOffsetX = 0.0;
  double containerOffsetY = 0.0;

  /*
   * Rows measured in this revision, UNDEFINED_INDEX until something is measured.
   */
  std::size_t measurementElementStartIndex = UNDEFINED_INDEX;
  std::size_t measurementElementEndIndex = UNDEFINED_INDEX;

  /*
   * Average row size, frozen once from real measurements.
   */
  double averageElementWidth = 0.0;
  double averageElementHeight = 0.0;

  /*
   * Count and total size of the rows measured so far, used to compute the average.
   */
  std::size_t measuredRealCount = 0;
  double measuredRealTotalWidth = 0.0;
  double measuredRealTotalHeight = 0.0;

  /*
   * Size of the visible viewport.
   */
  double windowContainerHeight = 0.0;
  double windowContainerWidth = 0.0;

  /*
   * Full scrollable content size.
   */
  double totalContainerHeight = 0.0;
  double totalContainerWidth = 0.0;
};

}
