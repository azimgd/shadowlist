#include <shadowlist-core/host/SwipeReveal.hpp>

#include <algorithm>
#include <cmath>

namespace azimgd::shadowlist {

void SwipeReveal::begin(const SwipeSpec& spec, double startOffset) {
  spec_ = spec;
  startOffset_ = startOffset;
}

double SwipeReveal::drag(double translation) const {
  double raw = startOffset_ + translation;
  bool leading = raw > 0.0;
  double width = leading ? spec_.leadingWidth : spec_.trailingWidth;
  if (width <= 0.0) {
    return 0.0;
  }
  bool full = leading ? spec_.leadingFullSwipe : spec_.trailingFullSwipe;
  double distance = std::fabs(raw);
  double shown = distance;
  if (full) {
    shown = std::min(distance, std::max(spec_.rowSize, width));
  } else if (distance > width) {
    shown = width + (distance - width) * SWIPE_RUBBER_BAND;
  }
  return leading ? shown : -shown;
}

bool SwipeReveal::isPastFullSwipe(double offset) const {
  bool leading = offset > 0.0;
  bool full = leading ? spec_.leadingFullSwipe : spec_.trailingFullSwipe;
  double width = leading ? spec_.leadingWidth : spec_.trailingWidth;
  if (!full || width <= 0.0) {
    return false;
  }
  double threshold = std::max(width, spec_.rowSize * SWIPE_FULL_FRACTION);
  return std::fabs(offset) >= threshold;
}

SwipeRest SwipeReveal::settle(double offset, double velocity, double flingVelocity) const {
  SwipeRest rest;
  if (offset == 0.0) {
    return rest;
  }
  bool leading = offset > 0.0;
  SwipeSide side = leading ? SwipeSide::Leading : SwipeSide::Trailing;
  double width = leading ? spec_.leadingWidth : spec_.trailingWidth;
  // Positive toward the side's open direction.
  double toward = leading ? velocity : -velocity;
  if (isPastFullSwipe(offset)) {
    rest.side = side;
    rest.full = true;
    rest.offset = leading ? std::max(spec_.rowSize, width) : -std::max(spec_.rowSize, width);
    return rest;
  }
  bool flungOpen = toward > flingVelocity;
  bool flungClosed = toward < -flingVelocity;
  if (width > 0.0 && !flungClosed && (flungOpen || std::fabs(offset) > width / 2.0)) {
    rest.side = side;
    rest.offset = openOffset(side);
  }
  return rest;
}

double SwipeReveal::openOffset(SwipeSide side) const {
  switch (side) {
    case SwipeSide::Leading:
      return spec_.leadingWidth;
    case SwipeSide::Trailing:
      return -spec_.trailingWidth;
    case SwipeSide::None:
      return 0.0;
  }
  return 0.0;
}

bool SwipeReveal::isSwipedOut(double offset) const {
  return spec_.rowSize > 0.0 && std::fabs(offset) >= spec_.rowSize - 1.0;
}

double swipeButtonSize(double fitted, double scale) {
  return std::max(SWIPE_BUTTON_MIN * scale, fitted + SWIPE_BUTTON_PADDING * scale);
}

void swipeButtonSpans(
  const std::vector<double>& sizes,
  double offset,
  bool full,
  double crossSize,
  std::vector<SwipeSpan>& out) {
  out.clear();
  out.reserve(sizes.size());
  bool leading = offset > 0.0;
  double gap = std::fabs(offset);
  double total = 0.0;
  for (double size : sizes) {
    total += size;
  }
  double scale = total > 0.0 ? gap / total : 0.0;
  // From the edge inward: the first action sits at the outer edge.
  double edge = 0.0;
  for (std::size_t at = 0; at < sizes.size(); ++at) {
    double size = full ? (at == 0 ? gap : 0.0) : sizes[at] * scale;
    double start = leading ? edge : crossSize - edge - size;
    out.push_back({start, size});
    edge += size;
  }
}

SwipeSpan swipeRevealedSpan(double offset, double crossSize) {
  double gap = std::fabs(offset);
  return {offset > 0.0 ? 0.0 : crossSize - gap, gap};
}

}
