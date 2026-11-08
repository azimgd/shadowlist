#pragma once

#include <shadowlist-core/Operation.hpp>

#include <functional>

namespace azimgd::shadowlist {

/*
 * A pan that starts within this many degrees of an axis moves that axis only, otherwise both.
 */
constexpr double SCROLL_LOCK_DEGREES = 30.0;

/*
 * Rubber banding: a pull past an edge meets more resistance the further it goes and stops at
 * SCROLL_ELASTIC_LIMIT of the viewport. Momentum that reaches an edge goes at most
 * SCROLL_MOMENTUM_ELASTIC_LIMIT of the viewport in, meets SCROLL_MOMENTUM_RESISTANCE of its
 * force there and stops at half that share. A sheet past an edge springs back with
 * SCROLL_SPRING_TIME_MS as its time constant.
 */
constexpr double SCROLL_ELASTIC_LIMIT = 0.5;
constexpr double SCROLL_MOMENTUM_ELASTIC_LIMIT = 0.12;
constexpr double SCROLL_MOMENTUM_RESISTANCE = 0.35;
constexpr double SCROLL_SPRING_TIME_MS = 75.0;

/*
 * How long an animated scroll takes, easing out, in milliseconds.
 */
constexpr double SCROLL_ANIMATION_DURATION_MS = 300.0;

/*
 * How long a scroll that came to rest between snap points takes to ease onto one, in
 * milliseconds.
 */
constexpr double SCROLL_SETTLE_DURATION_MS = 250.0;

/*
 * A mouse wheel never says it ended. It has once it is quiet for SCROLL_WHEEL_IDLE_MS. Fingers
 * that lift without momentum following within SCROLL_MOMENTUM_WAIT_MS end their gesture.
 */
constexpr double SCROLL_WHEEL_IDLE_MS = 150.0;
constexpr double SCROLL_MOMENTUM_WAIT_MS = 80.0;

/*
 * How far one line of a mouse wheel scrolls, in points.
 */
constexpr double SCROLL_WHEEL_LINE = 16.0;

/*
 * The frame length assumed for the first frame of a run, in milliseconds.
 */
constexpr double SCROLL_FRAME_MS = 1000.0 / 120.0;

/*
 * Which axes a pan moves, decided once from its first movement: within lockDegrees of an axis
 * it moves that axis only, otherwise both.
 */
struct AxisLock {
  bool x = true;
  bool y = true;
};

AxisLock lockAxes(double deltaX, double deltaY, double lockDegrees);

/*
 * How far an offset may go past [0, max] under a pull of delta: the part inside moves freely
 * and the part past an edge meets the rubber band's resistance, which stops it at limitShare of
 * dimension.
 */
double elasticStep(double value, double delta, double max, double dimension, double limitShare);

/*
 * One frame of a spring back toward [0, max]: the distance past the edge keeps `keep` of itself
 * and snaps to the edge under half a point.
 */
double springStep(double value, double max, double keep);

/*
 * The share of the distance past an edge a spring keeps over a frame of intervalMs.
 */
double springKeep(double intervalMs);

/*
 * Where an animation started at progress 0 is at progress t in [0, 1], easing out.
 */
double easeOut(double t);

/*
 * A scroller along one axis of content `total` long shown `view` at a time at `offset`: its start
 * from the view's edge and its length, at least minLength. The track starts at trackStart and is
 * inset from both ends. Hidden when the axis does not scroll. Content past an edge shortens it,
 * the way AppKit's scrollers do.
 */
struct ScrollerThumb {
  bool visible = false;
  double start = 0.0;
  double length = 0.0;

  double end() const { return start + length; }
};

ScrollerThumb scrollerThumb(
  double total,
  double view,
  double offset,
  double trackStart,
  double inset,
  double minLength);

/*
 * The offset a dragged thumb puts the content at: where its start, held grabOffset from the
 * pointer, is along the free part of its track.
 */
double scrollerDragOffset(
  double pointer,
  double grabOffset,
  double trackStart,
  double inset,
  double view,
  double thumbLength,
  double maxOffset);

/*
 * Where along a thumb a pointer that pressed at `along` holds it: where it pressed when on the
 * thumb, otherwise its middle, which jumps there.
 */
double scrollerGrabOffset(const ScrollerThumb& thumb, double along);

/*
 * The phase of a wheel event or of its momentum, the same as AppKit's NSEventPhase.
 */
enum class WheelPhase {
  None,
  MayBegin,
  Began,
  Changed,
  Stationary,
  Ended,
  Cancelled,
};

/*
 * One scroll wheel event. Deltas are how far the content moves toward its end, in points: a
 * mouse wheel's lines already multiplied by SCROLL_WHEEL_LINE.
 */
struct WheelEvent {
  WheelPhase phase = WheelPhase::None;
  WheelPhase momentum = WheelPhase::None;
  double deltaX = 0.0;
  double deltaY = 0.0;
};

/*
 * What a call to ScrollPhysics changed: the offset moved, the phase changed, an animation
 * started with animateTo reached its target. handled is false for a wheel event the host passes
 * on to what is behind the view: a mouse wheel that moves nothing and every event of a gesture
 * across a one axis view.
 */
struct ScrollStep {
  bool handled = true;
  bool moved = false;
  bool phaseChanged = false;
  bool animationEnded = false;
};

/*
 * The scrolling of a view on one or two axes, driven by scroll wheel events and frames. Offsets
 * run from 0 at the content start to max at its end, and rubber band past them.
 *
 * - A trackpad gesture is Dragging from its first movement. On two axes it moves the axes
 *   lockAxes picks from that movement. On one axis a gesture that starts more across it than
 *   along it is not handled, its momentum included. It rubber bands past the edges. Momentum the
 *   system sends after it is Settling, and momentum that reaches an edge springs back. Fingers
 *   that lift past an edge spring back too. Fingers that touch a settling view stop it.
 * - A mouse wheel moves by lines, stops at the edges and is Dragging until it is quiet.
 * - animateTo eases to an offset over SCROLL_ANIMATION_DURATION_MS in the Idle phase. While
 *   fingers are on the trackpad it does nothing, as fingers stop a running animation too.
 * - A mouse wheel turned during trackpad momentum drops the rest of that momentum.
 * - When a gesture comes to rest the SnapOffset callback may move its resting offset. The
 *   physics animates there and stays Settling until it arrives.
 *
 * Times are in milliseconds from any fixed start. step runs a frame while needsFrames is true.
 */
class ScrollPhysics final {
public:
  using SnapOffset = std::function<void(double& x, double& y)>;

  void setAxes(bool x, bool y);
  void setLimits(double maxX, double maxY, double viewportX, double viewportY);
  void setSnapOffset(SnapOffset snapOffset);

  /*
   * Move to an offset the host set, like a correction. A gesture or momentum keeps going from
   * there, and a running animation keeps its distance to the target.
   */
  void setOffset(double x, double y);

  double getOffsetX() const { return x_; }
  double getOffsetY() const { return y_; }
  double getMaxX() const { return maxX_; }
  double getMaxY() const { return maxY_; }
  ScrollPhase getPhase() const { return phase_; }
  bool isAnimating() const { return animating_ && !snapping_; }
  bool isPastEdge() const;
  bool needsFrames() const;

  ScrollStep wheel(const WheelEvent& event, double now);
  ScrollStep step(double now);
  ScrollStep animateTo(double x, double y, double now);

  /*
   * Stop momentum, a spring and an animation, and rest inside the limits. Momentum still on its
   * way from the current gesture is dropped. Fingers on the trackpad keep their gesture.
   */
  ScrollStep stop();

private:
  void moveTo(double x, double y, ScrollStep& result);
  void setPhase(ScrollPhase phase, ScrollStep& result);
  void endTouch(ScrollStep& result, double now);
  void cancelTouch(ScrollStep& result, double now);
  void startSpring(ScrollStep& result);
  void rest(ScrollStep& result, double now);
  void startAnimation(double x, double y, double now);
  void stepAnimation(double now, ScrollStep& result);
  void stepSpring(double now, ScrollStep& result);
  void clearTimers();

  SnapOffset snapOffset_;
  bool axisX_ = false;
  bool axisY_ = true;
  double x_ = 0.0;
  double y_ = 0.0;
  double maxX_ = 0.0;
  double maxY_ = 0.0;
  double viewportX_ = 0.0;
  double viewportY_ = 0.0;
  ScrollPhase phase_ = ScrollPhase::Idle;

  /*
   * The gesture: fingers on the trackpad, the axes it moves once decided, whether its
   * momentum is dropped and whether it goes to whatever is behind the view.
   */
  bool touching_ = false;
  bool axisDecided_ = false;
  bool moveX_ = true;
  bool moveY_ = true;
  bool dropsMomentum_ = false;
  bool inMomentum_ = false;
  bool forwarding_ = false;

  /*
   * When the lifted fingers' momentum stops being waited for and when a quiet mouse wheel
   * ends, or a negative time.
   */
  double momentumDeadline_ = -1.0;
  double wheelDeadline_ = -1.0;

  bool springing_ = false;
  double previousFrame_ = -1.0;

  /*
   * An animation from one offset to another starting at animationStart_. A snap is an
   * animation that ends the gesture.
   */
  bool animating_ = false;
  bool snapping_ = false;
  double animationStart_ = 0.0;
  double fromX_ = 0.0;
  double fromY_ = 0.0;
  double toX_ = 0.0;
  double toY_ = 0.0;
};

}
