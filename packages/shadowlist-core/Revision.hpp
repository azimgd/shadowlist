#ifndef Revision_hpp
#define Revision_hpp

#include <string>
#include <chrono>
#include <shadowlist-core/Element.hpp>

namespace azimgd::shadowlist {

struct RevisionDebugRepresentationMetadata {
  /*
   * Time difference from previous revision (in milliseconds)
   */
  long long timestampDiff = 0;
};

class Revision {
public:
  /*
   * Elements array
   */
  std::vector<Element> elements;

  /*
   * Timestamp when revision was created
   */
  std::chrono::milliseconds timestamp;

  /*
   * Offset of the container within the viewport X axis
   */
  double containerOffsetX = 0.0f;

  /*
   * Offset of the container within the viewport Y axis
   */
  double containerOffsetY = 0.0f;

  /*
   * Start position of measurement loop
   */
  std::size_t measurementElementStartIndex = (std::size_t)-1;

  /*
   * End position of measurement loop
   */
  std::size_t measurementElementEndIndex = (std::size_t)-1;

  /*
   * How many elements did we measure so far
   */
  std::size_t measurementElementCount = 0;

  /*
   * Average element width
   */
  double averageElementWidth = 0.0f;

  /*
   * Average element height
   */
  double averageElementHeight = 0.0f;

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
  double windowContainerHeight = 0.0f;

  /*
   * Visible window width of the container
   */
  double windowContainerWidth = 0.0f;

  /*
   * Total height of the container
   */
  double totalContainerHeight = 0.0f;

  /*
   * Total width of the container
   */
  double totalContainerWidth = 0.0f;

  /*
   * Maintain visible content position diff height
   */
  double mvcpDiffHeight = 0.0f;

  /*
   * Maintain visible content position diff width
   */
  double mvcpDiffWidth = 0.0f;

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
  std::string getDebugRepresentation(const RevisionDebugRepresentationMetadata& metadata) const;
};

}
#endif
