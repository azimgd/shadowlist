#ifndef Scrollable_h
#define Scrollable_h

#include <react/renderer/graphics/Float.h>
#include <react/renderer/graphics/Point.h>
#include <react/renderer/graphics/Rect.h>
#include <react/renderer/graphics/Size.h>

namespace facebook::react {

class Scrollable final {
  public:
  static float getScrollPositionOffset(const Point& scrollPosition);
  static float getScrollContainerSize(const Size& scrollContainer);
  static float getScrollContentSize(const Size& scrollContent);
  static float getScrollContentItemSize(const Size& scrollContentItem);
  static int getVirtualizedOffset();
};

}

#endif
