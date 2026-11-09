#include <shadowlist-core/host/ScrollPhysics.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace azimgd::shadowlist {

namespace {

constexpr double PI = 3.14159265358979323846;

bool isPast(double value, double max) {
  return value < 0.0 || value > max;
}

}

AxisLock lockAxes(double deltaX, double deltaY, double lockDegrees) {
  double degrees = std::atan2(std::fabs(deltaY), std::fabs(deltaX)) * 180.0 / PI;
  return {degrees < 90.0 - lockDegrees, degrees > lockDegrees};
}

double elasticStep(double value, double delta, double max, double dimension, double limitShare) {
  double next = value + delta;
  if (next >= 0.0 && next <= max) {
    return delta;
  }
  bool below = next < 0.0;
  bool outward = below ? delta < 0.0 : delta > 0.0;
  if (!outward) {
    return delta;
  }
  double edge = below ? 0.0 : max;
  // The part of the step that reaches the edge moves freely, the rest meets the band.
  bool inside = value >= 0.0 && value <= max;
  double free = inside ? edge - value : 0.0;
  double beyond = inside ? 0.0 : std::fabs(value - edge);
  double limit = std::max(1.0, dimension * limitShare);
  double factor = beyond >= limit ? 0.0 : std::pow(1.0 - beyond / limit, 2);
  return free + (delta - free) * factor;
}

double springStep(double value, double max, double keep) {
  double edge = std::clamp(value, 0.0, std::max(0.0, max));
  double next = edge + (value - edge) * keep;
  return std::fabs(next - edge) < 0.5 ? edge : next;
}

double springKeep(double intervalMs) {
  return std::exp(-std::max(0.0, intervalMs) / SCROLL_SPRING_TIME_MS);
}

double easeOut(double t) {
  t = std::clamp(t, 0.0, 1.0);
  return 1.0 - std::pow(1.0 - t, 3);
}

ScrollerThumb scrollerThumb(
  double total,
  double view,
  double offset,
  double trackStart,
  double inset,
  double minLength) {
  ScrollerThumb thumb;
  if (!(total > view && view > 0.0)) {
    return thumb;
  }
  double maxOffset = total - view;
  double track = view - 2.0 * inset;
  double past = offset < 0.0 ? -offset : std::max(0.0, offset - maxOffset);
  thumb.visible = true;
  thumb.length = std::min(track, std::max(minLength, track * view / total - past));
  double progress = std::clamp(offset / std::max(1.0, maxOffset), 0.0, 1.0);
  thumb.start = trackStart + inset + progress * (track - thumb.length);
  return thumb;
}

double scrollerDragOffset(
  double pointer,
  double grabOffset,
  double trackStart,
  double inset,
  double view,
  double thumbLength,
  double maxOffset) {
  double free = view - 2.0 * inset - thumbLength;
  double progress = free > 0.0 ? std::clamp((pointer - grabOffset - trackStart - inset) / free, 0.0, 1.0) : 0.0;
  return progress * std::max(0.0, maxOffset);
}

double scrollerGrabOffset(const ScrollerThumb& thumb, double along) {
  bool onThumb = along >= thumb.start && along <= thumb.end();
  return onThumb ? along - thumb.start : thumb.length / 2.0;
}

void ScrollPhysics::setAxes(bool x, bool y) {
  axisX_ = x;
  axisY_ = y;
}

void ScrollPhysics::setLimits(double maxX, double maxY, double viewportX, double viewportY) {
  maxX_ = std::max(0.0, maxX);
  maxY_ = std::max(0.0, maxY);
  viewportX_ = viewportX;
  viewportY_ = viewportY;
}

void ScrollPhysics::setSnapOffset(SnapOffset snapOffset) {
  snapOffset_ = std::move(snapOffset);
}

void ScrollPhysics::setOffset(double x, double y) {
  if (animating_) {
    toX_ += x - x_;
    toY_ += y - y_;
    fromX_ += x - x_;
    fromY_ += y - y_;
  }
  x_ = x;
  y_ = y;
}

bool ScrollPhysics::isPastEdge() const {
  return (axisX_ && isPast(x_, maxX_)) || (axisY_ && isPast(y_, maxY_));
}

bool ScrollPhysics::needsFrames() const {
  return animating_ || (springing_ && !touching_) || momentumDeadline_ >= 0.0 || wheelDeadline_ >= 0.0;
}

void ScrollPhysics::moveTo(double x, double y, ScrollStep& result) {
  x = axisX_ ? x : x_;
  y = axisY_ ? y : y_;
  if (x == x_ && y == y_) {
    return;
  }
  x_ = x;
  y_ = y;
  result.moved = true;
}

void ScrollPhysics::setPhase(ScrollPhase phase, ScrollStep& result) {
  if (phase == phase_) {
    return;
  }
  phase_ = phase;
  result.phaseChanged = true;
}

void ScrollPhysics::clearTimers() {
  momentumDeadline_ = -1.0;
  wheelDeadline_ = -1.0;
}

ScrollStep ScrollPhysics::wheel(const WheelEvent& event, double now) {
  ScrollStep result;
  WheelPhase phase = event.phase;
  WheelPhase momentum = event.momentum;
  bool gesture = phase != WheelPhase::None || momentum != WheelPhase::None;
  if (phase == WheelPhase::MayBegin || phase == WheelPhase::Began) {
    // Fingers on the trackpad hold a gliding or springing view where it is.
    touching_ = true;
    springing_ = false;
    animating_ = false;
    snapping_ = false;
    inMomentum_ = false;
    axisDecided_ = false;
    dropsMomentum_ = false;
    forwarding_ = false;
    clearTimers();
    if (phase_ == ScrollPhase::Settling) {
      setPhase(ScrollPhase::Idle, result);
    }
  }
  if (forwarding_ && gesture) {
    result.handled = false;
    return result;
  }
  if (phase == WheelPhase::MayBegin) {
    return result;
  }
  if (phase == WheelPhase::Cancelled) {
    cancelTouch(result, now);
    return result;
  }
  if (momentum == WheelPhase::Began) {
    touching_ = false;
    momentumDeadline_ = -1.0;
    inMomentum_ = true;
    if (!dropsMomentum_) {
      setPhase(ScrollPhase::Settling, result);
    }
  }
  if (momentum != WheelPhase::None && dropsMomentum_) {
    if (momentum == WheelPhase::Ended || momentum == WheelPhase::Cancelled) {
      inMomentum_ = false;
    }
    return result;
  }

  double deltaX = event.deltaX;
  double deltaY = event.deltaY;
  bool oneAxis = axisX_ != axisY_;
  if (!gesture && axisX_ && !axisY_ && deltaX == 0.0) {
    // A mouse wheel turns a horizontal view with its vertical wheel.
    deltaX = deltaY;
  }
  if (gesture && !axisDecided_ && (deltaX != 0.0 || deltaY != 0.0)) {
    axisDecided_ = true;
    double along = std::fabs(axisX_ ? deltaX : deltaY);
    double across = std::fabs(axisX_ ? deltaY : deltaX);
    if (oneAxis && across > along) {
      // A gesture across the only axis belongs to whatever is behind the view, momentum too.
      forwarding_ = true;
      cancelTouch(result, now);
      result.handled = false;
      return result;
    }
    AxisLock lock = oneAxis ? AxisLock{} : lockAxes(deltaX, deltaY, SCROLL_LOCK_DEGREES);
    moveX_ = lock.x;
    moveY_ = lock.y;
    if (phase != WheelPhase::None) {
      setPhase(ScrollPhase::Dragging, result);
    }
  }
  if (gesture) {
    deltaX = moveX_ ? deltaX : 0.0;
    deltaY = moveY_ ? deltaY : 0.0;
  }
  deltaX = axisX_ ? deltaX : 0.0;
  deltaY = axisY_ ? deltaY : 0.0;

  double x = x_;
  double y = y_;
  if (!gesture) {
    // A mouse wheel takes over from trackpad momentum, which is dropped.
    if (inMomentum_ || momentumDeadline_ >= 0.0) {
      dropsMomentum_ = true;
      momentumDeadline_ = -1.0;
    }
    animating_ = false;
    snapping_ = false;
    springing_ = false;
    x = std::clamp(x + deltaX, 0.0, maxX_);
    y = std::clamp(y + deltaY, 0.0, maxY_);
    moveTo(x, y, result);
    if (!result.moved && phase_ != ScrollPhase::Dragging) {
      result.handled = false;
      return result;
    }
    setPhase(ScrollPhase::Dragging, result);
    wheelDeadline_ = now + SCROLL_WHEEL_IDLE_MS;
    return result;
  }
  if (momentum != WheelPhase::None) {
    double share = SCROLL_MOMENTUM_ELASTIC_LIMIT;
    x += elasticStep(x, deltaX, maxX_, viewportX_, share) * (isPast(x, maxX_) ? SCROLL_MOMENTUM_RESISTANCE : 1.0);
    y += elasticStep(y, deltaY, maxY_, viewportY_, share) * (isPast(y, maxY_) ? SCROLL_MOMENTUM_RESISTANCE : 1.0);
    moveTo(x, y, result);
    // Momentum that went far enough past an edge stops, and the view springs back.
    double stopX = viewportX_ * share * 0.5;
    double stopY = viewportY_ * share * 0.5;
    bool pastX = axisX_ && (x_ < -stopX || x_ > maxX_ + stopX);
    bool pastY = axisY_ && (y_ < -stopY || y_ > maxY_ + stopY);
    if (pastX || pastY) {
      dropsMomentum_ = true;
      startSpring(result);
    }
    if (momentum == WheelPhase::Ended || momentum == WheelPhase::Cancelled) {
      inMomentum_ = false;
      if (isPastEdge()) {
        startSpring(result);
      } else if (!springing_) {
        rest(result, now);
      }
    }
    return result;
  }
  x += elasticStep(x, deltaX, maxX_, viewportX_, SCROLL_ELASTIC_LIMIT);
  y += elasticStep(y, deltaY, maxY_, viewportY_, SCROLL_ELASTIC_LIMIT);
  moveTo(x, y, result);
  if (phase == WheelPhase::Ended) {
    endTouch(result, now);
  }
  return result;
}

/*
 * The fingers lifted. A view past an edge springs back and its momentum is dropped. Otherwise
 * the gesture waits a moment for its momentum.
 */
void ScrollPhysics::endTouch(ScrollStep& result, double now) {
  touching_ = false;
  if (isPastEdge()) {
    dropsMomentum_ = true;
    startSpring(result);
    return;
  }
  momentumDeadline_ = now + SCROLL_MOMENTUM_WAIT_MS;
}

/*
 * The fingers left without a fling: back inside the edges and rest.
 */
void ScrollPhysics::cancelTouch(ScrollStep& result, double now) {
  touching_ = false;
  dropsMomentum_ = true;
  if (isPastEdge()) {
    startSpring(result);
  } else {
    rest(result, now);
  }
}

void ScrollPhysics::startSpring(ScrollStep& result) {
  springing_ = true;
  momentumDeadline_ = -1.0;
  previousFrame_ = -1.0;
  setPhase(ScrollPhase::Settling, result);
}

/*
 * The gesture came to rest. A snap offset that differs moves it there first.
 */
void ScrollPhysics::rest(ScrollStep& result, double now) {
  clearTimers();
  springing_ = false;
  if (snapOffset_ && !snapping_) {
    double x = x_;
    double y = y_;
    snapOffset_(x, y);
    x = axisX_ ? std::clamp(x, 0.0, maxX_) : x_;
    y = axisY_ ? std::clamp(y, 0.0, maxY_) : y_;
    if (std::fabs(x - x_) >= 0.5 || std::fabs(y - y_) >= 0.5) {
      startAnimation(x, y, now);
      snapping_ = true;
      setPhase(ScrollPhase::Settling, result);
      return;
    }
  }
  snapping_ = false;
  setPhase(ScrollPhase::Idle, result);
}

void ScrollPhysics::startAnimation(double x, double y, double now) {
  animating_ = true;
  animationStart_ = now;
  fromX_ = x_;
  fromY_ = y_;
  toX_ = x;
  toY_ = y;
}

ScrollStep ScrollPhysics::animateTo(double x, double y, double now) {
  if (touching_) {
    // Fingers on the trackpad hold the view. The animation does not start.
    return ScrollStep{};
  }
  ScrollStep result = stop();
  x = axisX_ ? std::clamp(x, 0.0, maxX_) : x_;
  y = axisY_ ? std::clamp(y, 0.0, maxY_) : y_;
  startAnimation(x, y, now);
  return result;
}

ScrollStep ScrollPhysics::stop() {
  ScrollStep result;
  if (touching_) {
    return result;
  }
  if (inMomentum_ || momentumDeadline_ >= 0.0) {
    dropsMomentum_ = true;
  }
  animating_ = false;
  snapping_ = false;
  springing_ = false;
  clearTimers();
  moveTo(std::clamp(x_, 0.0, maxX_), std::clamp(y_, 0.0, maxY_), result);
  setPhase(ScrollPhase::Idle, result);
  return result;
}

ScrollStep ScrollPhysics::step(double now) {
  ScrollStep result;
  if (animating_) {
    stepAnimation(now, result);
  } else if (springing_ && !touching_) {
    stepSpring(now, result);
  }
  if (momentumDeadline_ >= 0.0 && now >= momentumDeadline_) {
    // The fingers lifted without momentum.
    momentumDeadline_ = -1.0;
    rest(result, now);
  }
  if (wheelDeadline_ >= 0.0 && now >= wheelDeadline_) {
    wheelDeadline_ = -1.0;
    rest(result, now);
  }
  previousFrame_ = needsFrames() ? now : -1.0;
  return result;
}

void ScrollPhysics::stepAnimation(double now, ScrollStep& result) {
  double t = (now - animationStart_) / SCROLL_ANIMATION_DURATION_MS;
  double eased = easeOut(t);
  moveTo(fromX_ + (toX_ - fromX_) * eased, fromY_ + (toY_ - fromY_) * eased, result);
  if (t < 1.0) {
    return;
  }
  animating_ = false;
  if (snapping_) {
    snapping_ = false;
    setPhase(ScrollPhase::Idle, result);
    return;
  }
  result.animationEnded = true;
}

/*
 * One frame of the spring back: past an edge, the distance to it shrinks with the time constant.
 */
void ScrollPhysics::stepSpring(double now, ScrollStep& result) {
  double interval = previousFrame_ >= 0.0 ? now - previousFrame_ : SCROLL_FRAME_MS;
  double keep = springKeep(interval);
  moveTo(springStep(x_, maxX_, keep), springStep(y_, maxY_, keep), result);
  if (!isPastEdge()) {
    rest(result, now);
  }
}

}
