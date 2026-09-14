#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Element.hpp>

namespace azimgd::shadowlist {

class Revision {
public:
  std::vector<Element> elements;

  /*
   * key -> index into `elements`, maintained by Virtualizer::reconcileElements. Lets
   * findElementIndexByKey resolve an anchor in O(1) instead of scanning. On a duplicate key
   * the first occurrence wins.
   *
   * Values are stored biased: what a node holds is `trueIndex + indexBias`, not the index
   * itself. Always go through indexForKey / setIndexForKey. A raw read of `second` yields
   * a wrong index silently, which surfaces as rows writing their measured sizes onto other
   * rows.
   */
  std::unordered_map<std::string, std::size_t> elementIndexByKey;

  /*
   * Offset folded into every stored value, so a prepend does not have to rewrite them.
   *
   * Prepending K rows raises every surviving row's index by K. Walking the map to add K to
   * each value is an O(rows) chase through scattered nodes; subtracting K from this single
   * number has the identical effect in O(1), because every stored value is read back
   * through it. Unsigned wraparound is well defined and self-consistent: the bias and the
   * stored values wrap together, so the difference is always right.
   *
   * A full rebuild resets it to zero and stores true indices.
   */
  std::size_t indexBias = 0;

  // Resolve a key to its element index, or UNDEFINED_INDEX when absent.
  std::size_t indexForKey(const std::string& key) const {
    auto entry = this->elementIndexByKey.find(key);
    if (entry == this->elementIndexByKey.end()) {
      return UNDEFINED_INDEX;
    }
    return entry->second - this->indexBias;
  }

  /*
   * Record `key` at `index`, keeping the first occurrence of a duplicate (matching the
   * rebuild path's emplace). Returns whether this was the first occurrence.
   */
  bool setIndexForKey(const std::string& key, std::size_t index) {
    return this->elementIndexByKey.emplace(key, index + this->indexBias).second;
  }

  // Current scroll offset.
  double containerOffsetX = 0.0;
  double containerOffsetY = 0.0;

  // Range of indices measured in this revision (UNDEFINED_INDEX until measured).
  std::size_t measurementElementStartIndex = UNDEFINED_INDEX;
  std::size_t measurementElementEndIndex = UNDEFINED_INDEX;

  // Number of elements measured in this revision.
  std::size_t measurementElementCount = 0;

  // Average element size, frozen once from real measurements (see recomputeTotalSize).
  double averageElementWidth = 0.0;
  double averageElementHeight = 0.0;

  /*
   * Running count and total size of natively measured elements. The frozen average is
   * computed from this real sample, so unmeasured elements are sized from real data.
   */
  std::size_t measuredRealCount = 0;
  double measuredRealTotalWidth = 0.0;
  double measuredRealTotalHeight = 0.0;

  // Total size of the elements measured in this revision.
  double measurementElementTotalHeight = 0;
  double measurementElementTotalWidth = 0;

  // Size of the visible window (the scroll viewport).
  double windowContainerHeight = 0.0;
  double windowContainerWidth = 0.0;

  // Total scrollable size of the container.
  double totalContainerHeight = 0.0;
  double totalContainerWidth = 0.0;

  void setWindowContainerHeight(double windowContainerHeight);
  void setWindowContainerWidth(double windowContainerWidth);
  void setContainerOffsetX(double containerOffsetX);
  void setContainerOffsetY(double containerOffsetY);

  // Serialize the revision to a JSON string for debugging.
  std::string getDebugRepresentation() const;
};

}
