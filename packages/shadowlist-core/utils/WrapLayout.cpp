#include <shadowlist-core/utils/WrapLayout.hpp>

#include <algorithm>
#include <cmath>

namespace azimgd::shadowlist {

WrapLayout wrapLayout(double width, const std::vector<Size>& sizes, double gap) {
  WrapLayout layout;
  if (!(width > 0.0) || !std::isfinite(width)) return layout;
  gap = std::isfinite(gap) ? std::max(0.0, gap) : 0.0;
  double x = 0.0;
  double y = 0.0;
  double lineHeight = 0.0;
  bool hasChild = false;
  layout.frames.reserve(sizes.size());
  for (const Size& size : sizes) {
    double childWidth = std::isfinite(size.width) ? std::max(0.0, size.width) : 0.0;
    double childHeight = std::isfinite(size.height) ? std::max(0.0, size.height) : 0.0;
    if (hasChild && x + childWidth > width) {
      x = 0.0;
      y += lineHeight + gap;
      lineHeight = 0.0;
      hasChild = false;
    }
    layout.frames.push_back({x, y, childWidth, childHeight});
    x += childWidth + gap;
    lineHeight = std::max(lineHeight, childHeight);
    hasChild = true;
  }
  layout.height = y + lineHeight;
  return layout;
}

}
