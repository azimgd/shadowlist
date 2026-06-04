#ifndef Virtualizer_hpp
#define Virtualizer_hpp

#include <cstddef>
#include <string>
#include <vector>
#include <utility>

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Element.hpp>
#include <shadowlist-core/Error.hpp>

namespace azimgd::shadowlist {

/*
 * Platform agnostic description of a single frame. Integrations marshal their
 * props/state into this struct and read the resulting layout back from the
 * container, so the per-platform glue stays thin.
 */
struct FrameInput {
  std::vector<std::string> keys;
  double containerOffsetX = 0.0;
  double containerOffsetY = 0.0;
  double windowContainerWidth = 0.0;
  double windowContainerHeight = 0.0;
  double headerSize = 0.0;
  double footerSize = 0.0;
  bool inverted = false;
  bool horizontal = false;
  std::size_t columns = 1;
  std::pair<double, double> estimatedElementSize = {120.0, 120.0};
};

class Virtualizer {
public:
  /*
   * Single per-frame entry point: reconcile elements to the incoming keys,
   * measure, resolve scroll corrections (scrollToIndex / maintain visible
   * content position) and dispatch observer callbacks
   */
  void update(Container *container, const FrameInput &input);

  /*
   * Measure elements for the current revision
   * Handles default/inverted order and single/multi-column layout
   */
  void measure(Container *container);

  /*
   * Reconcile the element list to a new ordered set of keys, preserving the
   * measured state of surviving elements and creating fresh ones for new keys
   */
  static void reconcileElements(Container *container, const std::vector<std::string> &nextKeys);

  /*
   * Add element at specific index
   */
  static void addElementAtIndex(Container *container, std::size_t index);

  /*
   * Update measurements for existing element at specific index
   */
  static void updateElementAtIndex(Container *container, std::size_t index, Size size);

  /*
   * Prepend multiple elements to the beginning of the list
   */
  static void prependElements(Container *container, std::size_t count);

  /*
   * Append multiple elements to the end of the list
   */
  static void appendElements(Container *container, std::size_t count);

private:
  /*
   * Measure a window of elements from the edge of the list (first revision)
   */
  static void measureFirstRevision(Container *container);

  /*
   * Measure the elements within the visible window plus buffer (subsequent revisions)
   */
  static void measureNextRevision(Container *container);

  /*
   * Store the measured index range (orientation aware) and update average dimensions
   */
  static void finalizeMeasurement(Container *container, std::size_t measuredMinIndex, std::size_t measuredMaxIndex);

  /*
   * Size unmeasured elements with average dimensions and recompute all offsets
   */
  static void layoutElements(Container *container);

  /*
   * Recompute element offsets starting from a given index (orientation/columns aware)
   */
  static void recomputeElementOffsets(Container *container, std::size_t fromIndex);

  /*
   * Recompute total container size from the maximum element extent
   */
  static void recomputeTotalSize(Container *container);

  /*
   * Recompute total size and, on the first revision of an inverted list,
   * position the container at the bottom/right
   */
  static void finalizeContainer(Container *container);

  /*
   * Record which element currently sits at the top/left of the viewport and how
   * far we are scrolled into it, so the position can be restored after a reconcile
   */
  static void captureAnchor(Container *container, double inputOffset, std::string &anchorKey, double &anchorDelta);

  /*
   * Apply scroll corrections after measuring: a pending scrollToIndex, otherwise
   * keep the captured anchor element at the same viewport position (MVCP)
   */
  static void resolveScroll(Container *container, const std::string &anchorKey, double anchorDelta, bool hadElementsBefore);
};

}
#endif
