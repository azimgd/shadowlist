#ifndef Scrollable_h
#define Scrollable_h

#include <react/renderer/graphics/Float.h>
#include <react/renderer/graphics/Point.h>
#include <react/renderer/graphics/Rect.h>
#include <react/renderer/graphics/Size.h>

namespace facebook::react {

class Scrollable final {
  public:
  static float getScrollPositionOffset(const Point& scrollPosition, bool horizontal);
  static float getScrollContainerSize(const Size& scrollContainer, bool horizontal);
  static float getScrollContentSize(const Size& scrollContent, bool horizontal);
  static float getScrollContentItemSize(const Size& scrollContentItem, bool horizontal);
  static int getVirtualizedOffset();
};

}

#endif
