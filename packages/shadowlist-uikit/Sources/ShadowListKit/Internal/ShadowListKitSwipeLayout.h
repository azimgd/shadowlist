#pragma once

#import "ShadowListKitPlatform.h"

#include <shadowlist-core/host/SwipeReveal.hpp>

#include <numeric>
#include <vector>

/*
 * Where a swipe's buttons and the gap they show through sit for a row moved offset across the
 * axis, both kits' actions views alike. The spans come from the core's swipeButtonSpans and
 * swipeRevealedSpan. Frames are in the actions view's bounds, the buttons in the order of
 * widths, and a horizontal list stacks them down its cross axis.
 */
struct ShadowListKitSwipeLayout {
  std::vector<CGRect> buttons;
  CGRect gap = CGRectZero;
};

static inline CGFloat ShadowListKitSwipeWidth(const std::vector<double>& widths)
{
  return (CGFloat)std::accumulate(widths.begin(), widths.end(), 0.0);
}

static inline void ShadowListKitLayOutSwipe(
  const std::vector<double>& widths,
  CGFloat offset,
  BOOL full,
  CGSize size,
  BOOL horizontal,
  std::vector<azimgd::shadowlist::SwipeSpan>& spans,
  ShadowListKitSwipeLayout& layout)
{
  CGFloat crossSize = horizontal ? size.height : size.width;
  CGFloat alongSize = horizontal ? size.width : size.height;
  azimgd::shadowlist::swipeButtonSpans(widths, offset, full, crossSize, spans);
  layout.buttons.clear();
  for (const azimgd::shadowlist::SwipeSpan& span : spans) {
    CGFloat start = (CGFloat)span.start;
    CGFloat width = (CGFloat)span.size;
    layout.buttons.push_back(horizontal ? CGRectMake(0, start, alongSize, width) : CGRectMake(start, 0, width, alongSize));
  }
  azimgd::shadowlist::SwipeSpan gap = azimgd::shadowlist::swipeRevealedSpan(offset, crossSize);
  layout.gap = horizontal ? CGRectMake(0, (CGFloat)gap.start, alongSize, (CGFloat)gap.size)
                          : CGRectMake((CGFloat)gap.start, 0, (CGFloat)gap.size, alongSize);
}
