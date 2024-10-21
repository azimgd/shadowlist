#include "SLContainerState.h"

namespace facebook::react {

SLContainerState::SLContainerState(
  SLFenwickTree childrenMeasurements,
  Point scrollPosition,
  Size scrollContainer,
  Size scrollContent,
  int visibleStartIndex,
  int visibleEndIndex) :
    childrenMeasurements(childrenMeasurements),
    scrollPosition(scrollPosition),
    scrollContainer(scrollContainer),
    scrollContent(scrollContent),
    visibleStartIndex(visibleStartIndex),
    visibleEndIndex(visibleEndIndex) {}

#ifdef ANDROID
folly::dynamic SLContainerState::getDynamic() const {
  folly::dynamic dynamicScrollContent = folly::dynamic::object;
  dynamicScrollContent["width"] = scrollContent.width;
  dynamicScrollContent["height"] = scrollContent.height;

  return folly::dynamic::object(
    "scrollContent",
    dynamicScrollContent
  );
}
#endif

int SLContainerState::calculateVisibleStartIndex(float visibleStartOffset) {
  int offset = 5;
  int visibleStartIndex = childrenMeasurements.lower_bound(visibleStartOffset);
  int visibleEndIndexMin = 0;
  return std::max(visibleStartIndex - offset, visibleEndIndexMin);
}

int SLContainerState::calculateVisibleEndIndex(float visibleEndOffset) {
  int offset = 5;
  int visibleEndIndex = childrenMeasurements.lower_bound(visibleEndOffset);
  int visibleEndIndexMax = childrenMeasurements.size();
  return std::min(visibleEndIndex + offset, visibleEndIndexMax);
}

float SLContainerState::calculateContentSize() {
  return childrenMeasurements.sum(childrenMeasurements.size());
}

}
