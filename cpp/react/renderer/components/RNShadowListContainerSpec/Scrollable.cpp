#include "Scrollable.h"

namespace facebook::react {

float Scrollable::getScrollPositionOffset(const Point& scrollPosition) {
  return scrollPosition.y;
}

float Scrollable::getScrollContentSize(const Size& scrollContent) {
  return scrollContent.height;
}

float Scrollable::getScrollContainerSize(const Size& scrollContainer) {
  return scrollContainer.height;
}

float Scrollable::getScrollContentItemSize(const Size& scrollContentItem) {
  return scrollContentItem.height;
}

int Scrollable::getVirtualizedOffset() {
  return 10;
}

}
