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

}
