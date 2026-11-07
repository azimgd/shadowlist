/*
 * Scroll physics tests. Wheel gestures move a view, rubber band past its edges and spring back,
 * a mouse wheel stops at the edges and animations ease to their targets.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/ScrollPhysics.hpp>

#include <cmath>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

/*
 * A vertical view 500 tall over 2000 of content: the offset runs from 0 to 1500.
 */
ScrollPhysics verticalPhysics() {
  ScrollPhysics physics;
  physics.setAxes(false, true);
  physics.setLimits(0.0, 1500.0, 300.0, 500.0);
  return physics;
}

ScrollPhysics gridPhysics() {
  ScrollPhysics physics;
  physics.setAxes(true, true);
  physics.setLimits(1000.0, 1500.0, 400.0, 500.0);
  return physics;
}

WheelEvent gestureEvent(WheelPhase phase, double deltaX, double deltaY) {
  WheelEvent event;
  event.phase = phase;
  event.deltaX = deltaX;
  event.deltaY = deltaY;
  return event;
}

WheelEvent momentumEvent(WheelPhase momentum, double deltaX, double deltaY) {
  WheelEvent event;
  event.momentum = momentum;
  event.deltaX = deltaX;
  event.deltaY = deltaY;
  return event;
}

/*
 * Run frames every 8 ms from now until the physics needs none, or a second passed. Returns
 * the time after the last frame.
 */
double runFrames(ScrollPhysics& physics, double now) {
  double end = now + 1000.0;
  while (physics.needsFrames() && now < end) {
    now += 8.0;
    physics.step(now);
  }
  return now;
}

}

TEST(scroll_pans_lock_to_an_axis_within_the_lock_angle) {
  AxisLock flat = lockAxes(10, 2, SCROLL_LOCK_DEGREES);
  CHECK(flat.x && !flat.y);
  AxisLock steep = lockAxes(1, 10, SCROLL_LOCK_DEGREES);
  CHECK(!steep.x && steep.y);
  AxisLock diagonal = lockAxes(10, 10, SCROLL_LOCK_DEGREES);
  CHECK(diagonal.x && diagonal.y);
  AxisLock past = lockAxes(1, std::tan(80 * 3.14159265358979323846 / 180), SCROLL_LOCK_DEGREES);
  CHECK(!past.x && past.y);
}

TEST(scroll_rubber_band_resists_past_the_edges_and_springs_back) {
  // Inside the content a pull moves freely.
  CHECK_NEAR(elasticStep(50, 10, 100, 300, 0.5), 10, 1e-9);
  // Reaching the edge, the part inside is free and the rest meets the band at full strength.
  CHECK_NEAR(elasticStep(95, 10, 100, 300, 0.5), 10, 1e-9);
  // Past the edge the pull weakens with the square of the distance left to the limit.
  CHECK_NEAR(elasticStep(100 + 75, 10, 100, 300, 0.5), 10 * 0.25, 1e-9);
  CHECK_NEAR(elasticStep(100 + 150, 10, 100, 300, 0.5), 0, 1e-9);
  // Pulling back in is free.
  CHECK_NEAR(elasticStep(-50, 20, 100, 300, 0.5), 20, 1e-9);
  CHECK_NEAR(springStep(-100, 500, 0.5), -50, 1e-9);
  CHECK_NEAR(springStep(500.4, 500, 0.5), 500, 1e-9);
  CHECK_NEAR(springStep(250, 500, 0.5), 250, 1e-9);
  CHECK_NEAR(springKeep(SCROLL_SPRING_TIME_MS), std::exp(-1.0), 1e-9);
}

TEST(scroll_animation_eases_out) {
  CHECK_NEAR(easeOut(0), 0, 1e-9);
  CHECK_NEAR(easeOut(0.5), 0.875, 1e-9);
  CHECK_NEAR(easeOut(1), 1, 1e-9);
  CHECK_NEAR(easeOut(2), 1, 1e-9);
}

TEST(scroll_a_trackpad_gesture_drags_and_its_momentum_settles) {
  ScrollPhysics physics = verticalPhysics();
  // Fingers that rest on the trackpad do not drag yet.
  ScrollStep began = physics.wheel(gestureEvent(WheelPhase::Began, 0, 0), 0);
  CHECK(!began.phaseChanged);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
  ScrollStep moved = physics.wheel(gestureEvent(WheelPhase::Changed, 3, 40), 8);
  CHECK(moved.phaseChanged && moved.moved);
  CHECK(physics.getPhase() == ScrollPhase::Dragging);
  physics.wheel(gestureEvent(WheelPhase::Changed, 0, 60), 16);
  CHECK_NEAR(physics.getOffsetY(), 100, 1e-9);
  // A one axis view ignores the cross axis.
  CHECK_NEAR(physics.getOffsetX(), 0, 1e-9);
  physics.wheel(gestureEvent(WheelPhase::Ended, 0, 0), 24);
  CHECK(physics.getPhase() == ScrollPhase::Dragging);
  physics.wheel(momentumEvent(WheelPhase::Began, 0, 30), 30);
  CHECK(physics.getPhase() == ScrollPhase::Settling);
  physics.wheel(momentumEvent(WheelPhase::Changed, 0, 20), 38);
  ScrollStep ended = physics.wheel(momentumEvent(WheelPhase::Ended, 0, 0), 46);
  CHECK(ended.phaseChanged);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
  CHECK_NEAR(physics.getOffsetY(), 150, 1e-9);
}

TEST(scroll_lifted_fingers_without_momentum_end_the_gesture_after_a_wait) {
  ScrollPhysics physics = verticalPhysics();
  physics.wheel(gestureEvent(WheelPhase::Began, 0, 10), 0);
  physics.wheel(gestureEvent(WheelPhase::Ended, 0, 0), 8);
  CHECK(physics.needsFrames());
  physics.step(8 + SCROLL_MOMENTUM_WAIT_MS - 1);
  CHECK(physics.getPhase() == ScrollPhase::Dragging);
  physics.step(8 + SCROLL_MOMENTUM_WAIT_MS);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
  CHECK(!physics.needsFrames());
}

TEST(scroll_a_drag_past_the_edge_rubber_bands_and_springs_back) {
  ScrollPhysics physics = verticalPhysics();
  physics.wheel(gestureEvent(WheelPhase::Began, 0, 0), 0);
  for (int index = 0; index < 20; ++index) {
    physics.wheel(gestureEvent(WheelPhase::Changed, 0, -40), 8.0 * index);
  }
  double pulled = physics.getOffsetY();
  CHECK(pulled < -20);
  // The band stops it at half the viewport.
  CHECK(pulled > -250);
  physics.wheel(gestureEvent(WheelPhase::Ended, 0, 0), 200);
  CHECK(physics.getPhase() == ScrollPhase::Settling);
  // Momentum after fingers that lifted past the edge is dropped.
  physics.wheel(momentumEvent(WheelPhase::Began, 0, -30), 205);
  CHECK_NEAR(physics.getOffsetY(), pulled, 1e-9);
  runFrames(physics, 205);
  CHECK_NEAR(physics.getOffsetY(), 0, 1e-9);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
}

TEST(scroll_momentum_reaching_an_edge_goes_a_little_way_in_and_springs_back) {
  ScrollPhysics physics = verticalPhysics();
  physics.setOffset(0, 1450);
  physics.wheel(gestureEvent(WheelPhase::Began, 0, 20), 0);
  physics.wheel(gestureEvent(WheelPhase::Ended, 0, 0), 8);
  physics.wheel(momentumEvent(WheelPhase::Began, 0, 30), 10);
  double deepest = physics.getOffsetY();
  double now = 10;
  for (int index = 0; index < 60; ++index) {
    now += 8;
    physics.wheel(momentumEvent(WheelPhase::Changed, 0, 30), now);
    physics.step(now);
    deepest = std::max(deepest, physics.getOffsetY());
  }
  // At most SCROLL_MOMENTUM_ELASTIC_LIMIT of the viewport past the end.
  CHECK(deepest > 1500);
  CHECK(deepest <= 1500 + 500 * SCROLL_MOMENTUM_ELASTIC_LIMIT);
  physics.wheel(momentumEvent(WheelPhase::Ended, 0, 0), now);
  runFrames(physics, now);
  CHECK_NEAR(physics.getOffsetY(), 1500, 1e-9);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
}

TEST(scroll_a_gesture_within_the_lock_angle_moves_one_axis_of_two) {
  ScrollPhysics physics = gridPhysics();
  physics.wheel(gestureEvent(WheelPhase::Began, 0, 0), 0);
  // 20 degrees from vertical: vertical only for the whole gesture.
  physics.wheel(gestureEvent(WheelPhase::Changed, 3.6, 10), 8);
  physics.wheel(gestureEvent(WheelPhase::Changed, 30, 10), 16);
  CHECK_NEAR(physics.getOffsetX(), 0, 1e-9);
  CHECK_NEAR(physics.getOffsetY(), 20, 1e-9);
  physics.wheel(gestureEvent(WheelPhase::Ended, 0, 0), 24);
  physics.wheel(momentumEvent(WheelPhase::Began, 20, 5), 30);
  CHECK_NEAR(physics.getOffsetX(), 0, 1e-9);
  CHECK_NEAR(physics.getOffsetY(), 25, 1e-9);
  physics.wheel(momentumEvent(WheelPhase::Ended, 0, 0), 40);

  // A diagonal gesture moves both.
  physics.wheel(gestureEvent(WheelPhase::Began, 0, 0), 100);
  physics.wheel(gestureEvent(WheelPhase::Changed, 10, 10), 108);
  CHECK_NEAR(physics.getOffsetX(), 10, 1e-9);
  CHECK_NEAR(physics.getOffsetY(), 35, 1e-9);
}

TEST(scroll_a_mouse_wheel_moves_within_the_edges_and_ends_when_quiet) {
  ScrollPhysics physics = verticalPhysics();
  ScrollStep first = physics.wheel(momentumEvent(WheelPhase::None, 0, 3 * SCROLL_WHEEL_LINE), 0);
  CHECK(first.handled && first.moved);
  CHECK(physics.getPhase() == ScrollPhase::Dragging);
  physics.wheel(momentumEvent(WheelPhase::None, 0, -10 * SCROLL_WHEEL_LINE), 10);
  CHECK_NEAR(physics.getOffsetY(), 0, 1e-9);
  physics.step(10 + SCROLL_WHEEL_IDLE_MS);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
  // At the edge a wheel that moves nothing is left to the next responder.
  ScrollStep stuck = physics.wheel(momentumEvent(WheelPhase::None, 0, -SCROLL_WHEEL_LINE), 400);
  CHECK(!stuck.handled);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
}

TEST(scroll_a_horizontal_view_turns_with_the_vertical_mouse_wheel) {
  ScrollPhysics physics;
  physics.setAxes(true, false);
  physics.setLimits(800, 0, 300, 100);
  physics.wheel(momentumEvent(WheelPhase::None, 0, 2 * SCROLL_WHEEL_LINE), 0);
  CHECK_NEAR(physics.getOffsetX(), 2 * SCROLL_WHEEL_LINE, 1e-9);
  CHECK_NEAR(physics.getOffsetY(), 0, 1e-9);
}

TEST(scroll_an_animation_lands_exactly_on_its_target) {
  ScrollPhysics physics = verticalPhysics();
  physics.animateTo(0, 900, 0);
  CHECK(physics.isAnimating());
  CHECK(physics.getPhase() == ScrollPhase::Idle);
  physics.step(SCROLL_ANIMATION_DURATION_MS / 2);
  CHECK(physics.getOffsetY() > 450);
  CHECK(physics.getOffsetY() < 900);
  ScrollStep end = physics.step(SCROLL_ANIMATION_DURATION_MS);
  CHECK(end.animationEnded);
  CHECK_EQ(physics.getOffsetY(), 900.0);
  CHECK(!physics.isAnimating());
  // A target past the end stops at it.
  physics.animateTo(0, 9000, 1000);
  runFrames(physics, 1000);
  CHECK_EQ(physics.getOffsetY(), 1500.0);
}

TEST(scroll_a_correction_during_an_animation_keeps_its_distance_to_the_target) {
  ScrollPhysics physics = verticalPhysics();
  physics.animateTo(0, 600, 0);
  physics.step(100);
  double before = physics.getOffsetY();
  physics.setOffset(0, before + 50);
  runFrames(physics, 100);
  CHECK_EQ(physics.getOffsetY(), 650.0);
}

TEST(scroll_fingers_on_the_trackpad_stop_an_animation) {
  ScrollPhysics physics = verticalPhysics();
  physics.animateTo(0, 600, 0);
  physics.step(100);
  double held = physics.getOffsetY();
  ScrollStep down = physics.wheel(gestureEvent(WheelPhase::MayBegin, 0, 0), 108);
  CHECK(!down.animationEnded);
  CHECK(!physics.isAnimating());
  physics.step(200);
  CHECK_EQ(physics.getOffsetY(), held);
}

TEST(scroll_stop_drops_the_rest_of_the_momentum) {
  ScrollPhysics physics = verticalPhysics();
  physics.wheel(gestureEvent(WheelPhase::Began, 0, 10), 0);
  physics.wheel(gestureEvent(WheelPhase::Ended, 0, 0), 8);
  physics.wheel(momentumEvent(WheelPhase::Began, 0, 30), 10);
  ScrollStep stopped = physics.stop();
  CHECK(stopped.phaseChanged);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
  ScrollStep late = physics.wheel(momentumEvent(WheelPhase::Changed, 0, 30), 18);
  CHECK(late.handled && !late.moved);
  CHECK_NEAR(physics.getOffsetY(), 40, 1e-9);
  physics.wheel(momentumEvent(WheelPhase::Ended, 0, 0), 26);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
}

TEST(scroll_a_resting_gesture_snaps_and_settles_until_it_arrives) {
  ScrollPhysics physics = verticalPhysics();
  physics.setSnapOffset([](double&, double& y) { y = std::round(y / 100.0) * 100.0; });
  physics.wheel(momentumEvent(WheelPhase::None, 0, 140), 0);
  double now = runFrames(physics, 0);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
  CHECK_EQ(physics.getOffsetY(), 100.0);
  physics.wheel(gestureEvent(WheelPhase::Began, 0, 70), now);
  physics.wheel(gestureEvent(WheelPhase::Ended, 0, 0), now + 8);
  physics.step(now + 8 + SCROLL_MOMENTUM_WAIT_MS);
  CHECK(physics.getPhase() == ScrollPhase::Settling);
  CHECK(!physics.isAnimating());
  runFrames(physics, now + 8 + SCROLL_MOMENTUM_WAIT_MS);
  CHECK_EQ(physics.getOffsetY(), 200.0);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
}

TEST(scroll_scroller_thumbs_follow_the_offset_and_drag_the_content) {
  // 500 of 2000 shown: a quarter of the 496 track.
  ScrollerThumb top = scrollerThumb(2000, 500, 0, 0, 2, 36);
  CHECK(top.visible);
  CHECK_NEAR(top.start, 2, 1e-9);
  CHECK_NEAR(top.length, 124, 1e-9);
  ScrollerThumb end = scrollerThumb(2000, 500, 1500, 0, 2, 36);
  CHECK_NEAR(end.end(), 498, 1e-9);
  // Past an edge the thumb shortens and stays on the track.
  ScrollerThumb pulled = scrollerThumb(2000, 500, -50, 0, 2, 36);
  CHECK_NEAR(pulled.length, 74, 1e-9);
  CHECK_NEAR(pulled.start, 2, 1e-9);
  CHECK(scrollerThumb(2000, 500, 0, 0, 2, 200).length >= 200);
  CHECK(!scrollerThumb(400, 500, 0, 0, 2, 36).visible);
  // The free track is 372 long. Its middle is half the offset range.
  CHECK_NEAR(scrollerDragOffset(2 + 186 + 10, 10, 0, 2, 500, 124, 1500), 750, 1e-9);
  CHECK_NEAR(scrollerDragOffset(-100, 0, 0, 2, 500, 124, 1500), 0, 1e-9);
  CHECK_NEAR(scrollerDragOffset(900, 0, 0, 2, 500, 124, 1500), 1500, 1e-9);
  CHECK_NEAR(scrollerGrabOffset(top, 50), 48, 1e-9);
  CHECK_NEAR(scrollerGrabOffset(top, 300), 62, 1e-9);
}

TEST(scroll_a_gesture_across_a_one_axis_view_goes_to_what_is_behind_it) {
  ScrollPhysics physics = verticalPhysics();
  CHECK(physics.wheel(gestureEvent(WheelPhase::Began, 0, 0), 0).handled);
  ScrollStep across = physics.wheel(gestureEvent(WheelPhase::Changed, -30, 10), 8);
  CHECK(!across.handled);
  CHECK(!physics.wheel(gestureEvent(WheelPhase::Changed, 0, 40), 16).handled);
  CHECK(!physics.wheel(gestureEvent(WheelPhase::Ended, 0, 0), 24).handled);
  CHECK(!physics.wheel(momentumEvent(WheelPhase::Began, 0, 30), 30).handled);
  CHECK(!physics.wheel(momentumEvent(WheelPhase::Ended, 0, 0), 40).handled);
  CHECK_NEAR(physics.getOffsetY(), 0, 1e-9);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
  CHECK(!physics.needsFrames());
  // The next gesture decides again.
  physics.wheel(gestureEvent(WheelPhase::Began, 0, 0), 100);
  CHECK(physics.wheel(gestureEvent(WheelPhase::Changed, 10, 30), 108).handled);
  CHECK_NEAR(physics.getOffsetY(), 30, 1e-9);
}

TEST(scroll_fingers_on_a_settling_view_stop_it) {
  ScrollPhysics physics = verticalPhysics();
  physics.wheel(gestureEvent(WheelPhase::Began, 0, 10), 0);
  physics.wheel(gestureEvent(WheelPhase::Ended, 0, 0), 8);
  physics.wheel(momentumEvent(WheelPhase::Began, 0, 30), 10);
  CHECK(physics.getPhase() == ScrollPhase::Settling);
  ScrollStep down = physics.wheel(gestureEvent(WheelPhase::MayBegin, 0, 0), 20);
  CHECK(down.phaseChanged);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
  physics.wheel(gestureEvent(WheelPhase::Cancelled, 0, 0), 30);
  CHECK(physics.getPhase() == ScrollPhase::Idle);
  CHECK(!physics.needsFrames());
}
