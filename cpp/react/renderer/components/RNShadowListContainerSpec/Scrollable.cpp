#include "Scrollable.h"

namespace facebook::react {

float Scrollable::getScrollPositionOffset(const Point& scrollPosition, bool horizontal) {
  if (horizontal) {
    return scrollPosition.x;
  } else {
    return scrollPosition.y;
  }
}

float Scrollable::getScrollContentSize(const Size& scrollContent, bool horizontal) {
  if (horizontal) {
    return scrollContent.width;
  } else {
    return scrollContent.height;
  }
}

float Scrollable::getScrollContainerSize(const Size& scrollContainer, bool horizontal) {
  if (horizontal) {
    return scrollContainer.width;
  } else {
    return scrollContainer.height;
  }
}

float Scrollable::getScrollContentItemSize(const Size& scrollContentItem, bool horizontal) {
  if (horizontal) {
    return scrollContentItem.width;
  } else {
    return scrollContentItem.height;
  }
}

int Scrollable::getVirtualizedOffset() {
  return 10;
}

}
