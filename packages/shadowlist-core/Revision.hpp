#pragma once

#include <string>
#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Element.hpp>

namespace azimgd::shadowlist {

class Revision {
public:
  /*
   * Elements array
   */
  std::vector<Element> elements;

  /*
   * Offset of the container within the viewport X axis
   */
  double containerOffsetX = 0.0;

  /*
   * Offset of the container within the viewport Y axis
   */
  double containerOffsetY = 0.0;

  /*
   * Start position of measurement loop
   */
  std::size_t measurementElementStartIndex = UNDEFINED_INDEX;

  /*
   * End position of measurement loop
   */
  std::size_t measurementElementEndIndex = UNDEFINED_INDEX;

  /*
   * How many elements did we measure so far
   */
  std::size_t measurementElementCount = 0;

  /*
   * Average element width
   */
  double averageElementWidth = 0.0;

  /*
   * Average element height
   */
  double averageElementHeight = 0.0;

  /*
   * Cumulative count and size of elements that were actually (natively) measured,
   * as opposed to the estimate-filled measurement window. The average element size
   * is frozen from this real sample (see Virtualizer::recomputeTotalSize) so the
   * unmeasured portion of the list is sized from real data, not the estimate.
   */
  std::size_t measuredRealCount = 0;
  double measuredRealTotalWidth = 0.0;
  double measuredRealTotalHeight = 0.0;

  /*
   * Accumulated height of the container
   */
  double measurementElementTotalHeight = 0;

  /*
   * Accumulated width of the container
   */
  double measurementElementTotalWidth = 0;

  /*
   * Visible window height of the container
   */
  double windowContainerHeight = 0.0;

  /*
   * Visible window width of the container
   */
  double windowContainerWidth = 0.0;

  /*
   * Total height of the container
   */
  double totalContainerHeight = 0.0;

  /*
   * Total width of the container
   */
  double totalContainerWidth = 0.0;

  /*
   * Update containers window height
   */
  void setWindowContainerHeight(double windowContainerHeight);

  /*
   * Update containers window width
   */
  void setWindowContainerWidth(double windowContainerWidth);

  /*
   * Update containers X offset
   */
  void setContainerOffsetX(double containerOffsetX);

  /*
   * Update containers Y offset
   */
  void setContainerOffsetY(double containerOffsetY);

  /*
   * Get debug representation as JSON string
   */
  std::string getDebugRepresentation() const;
};

}
