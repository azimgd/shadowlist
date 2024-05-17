#include "ShadowListContainerState.h"

namespace facebook::react {

ShadowListContainerState::ShadowListContainerState(
  Point scrollPosition,
  Size scrollContainer,
  Size scrollContent,
  ShadowListFenwickTree scrollContentTree) :

    scrollPosition(scrollPosition),
    scrollContainer(scrollContainer),
    scrollContent(scrollContent),
    scrollContentTree(scrollContentTree) {}

/*
 * Measure layout and children metrics
 */
ShadowListContainerExtendedMetrics ShadowListContainerState::calculateExtendedMetrics(
  Point scrollPosition,
  bool horizontal,
  bool inverted) const {

  auto virtualizedOffset = Scrollable::getVirtualizedOffset();
  auto scrollPositionOffset = Scrollable::getScrollPositionOffset(scrollPosition, horizontal);
  auto scrollContentSize = Scrollable::getScrollContentSize(scrollContent, horizontal);
  auto scrollContainerSize = Scrollable::getScrollContainerSize(scrollContainer, horizontal);
  
  auto visibleStartPixels = std::max<float>(0.f, static_cast<double>(scrollPositionOffset));
  auto visibleEndPixels = std::min<float>(scrollContentSize, scrollPositionOffset + scrollContainerSize);

  int visibleStartIndex = scrollContentTree.lower_bound(visibleStartPixels);
  visibleStartIndex = std::max(0, visibleStartIndex - virtualizedOffset);

  int visibleEndIndex = scrollContentTree.lower_bound(visibleEndPixels);
  visibleEndIndex = std::min(scrollContentTree.size(), size_t(visibleEndIndex + virtualizedOffset));

  int blankTopStartIndex = 0;
  int blankTopEndIndex = std::max(0, visibleStartIndex - 1);

  auto blankTopStartPixels = 0.0;
  auto blankTopEndPixels = scrollContentTree.sum(blankTopStartIndex, blankTopEndIndex);

  int blankBottomStartIndex = std::min(size_t(visibleEndIndex + 1), scrollContentTree.size());
  int blankBottomEndIndex = scrollContentTree.size();

  auto blankBottomStartPixels = scrollContentTree.sum(blankBottomStartIndex, scrollContentTree.size());
  auto blankBottomEndPixels = scrollContentTree.sum(0, scrollContentTree.size());

  return ShadowListContainerExtendedMetrics{
    visibleStartIndex,
    visibleEndIndex,
    visibleStartPixels,
    visibleEndPixels,
    blankTopStartIndex,
    blankTopEndIndex,
    blankTopStartPixels,
    blankTopEndPixels,
    blankBottomStartIndex,
    blankBottomEndIndex,
    blankBottomStartPixels,
    blankBottomEndPixels,
  };
}

/*
 * Measure layout
 */
ShadowListContainerLayoutMetrics ShadowListContainerState::calculateLayoutMetrics() const {
  auto height = scrollContentTree.sum(0, scrollContentTree.size());

  return ShadowListContainerLayoutMetrics{
    height
  };
}

float ShadowListContainerState::calculateItemOffset(int index) const {
  return scrollContentTree.sum(0, index);
}

int ShadowListContainerState::countTree() const {
  return scrollContentTree.size();
}

}
