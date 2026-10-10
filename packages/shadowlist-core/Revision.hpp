#pragma once

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Row.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace azimgd::shadowlist {

class Revision final {
public:
  std::vector<Row> rows;

  /*
   * Maps each key to its row index so anchors resolve without a scan. The first duplicate wins.
   * Stored values include indexBias. Always use indexOfKey and setIndexOfKey. Reading
   * a value directly gives a wrong index, and rows end up taking other rows' sizes.
   */
  std::unordered_map<std::string, std::size_t> rowIndexByKey;

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
  std::size_t indexOfKey(const std::string& key) const {
    auto entry = rowIndexByKey.find(key);
    if (entry == rowIndexByKey.end()) {
      return UNDEFINED_INDEX;
    }
    return entry->second - indexBias;
  }

  /*
   * Store the key's index unless the key is already there. Returns true if it was new.
   */
  bool setIndexOfKey(const std::string& key, std::size_t index) {
    return rowIndexByKey.emplace(key, index + indexBias).second;
  }

  double offsetX = 0.0;
  double offsetY = 0.0;

  /*
   * Rows measured in this revision, UNDEFINED_INDEX until something is measured.
   */
  std::size_t measuredLow = UNDEFINED_INDEX;
  std::size_t measuredHigh = UNDEFINED_INDEX;

  /*
   * Average row size, frozen once from real measurements.
   */
  double averageRowWidth = 0.0;
  double averageRowHeight = 0.0;

  /*
   * Count and total size of the rows measured so far, used to compute the average.
   */
  std::size_t measuredRealCount = 0;
  double measuredRealTotalWidth = 0.0;
  double measuredRealTotalHeight = 0.0;

  /*
   * Size of the visible viewport.
   */
  double windowHeight = 0.0;
  double windowWidth = 0.0;

  /*
   * Full scrollable content size.
   */
  double contentHeight = 0.0;
  double contentWidth = 0.0;
};

}
