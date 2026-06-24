#pragma once

#include <string>
#include <unordered_map>
#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Element.hpp>

namespace azimgd::shadowlist {

class Revision {
public:
  std::vector<Element> elements;

  /*
   * key -> index into `elements`, rebuilt whenever the element list changes
   * (Virtualizer::reconcileElements). Lets findElementIndexByKey resolve an anchor in
   * O(1) instead of scanning. On a duplicate key the first occurrence wins.
   */
  std::unordered_map<std::string, std::size_t> elementIndexByKey;

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
