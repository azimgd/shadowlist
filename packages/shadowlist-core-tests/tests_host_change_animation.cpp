/*
 * ChangeAnimation tests: what animatesChanges slides, fades in and fades out in both kits.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/ChangeAnimation.hpp>

#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(change_animation_first_capture_asks_for_the_screen) {
  ChangeAnimation animation;
  CHECK(!animation.isPending());
  CHECK(animation.capture({"a"}, {}));
  CHECK(animation.isPending());
  // A second change before the layout adds up and keeps the screen of the first.
  CHECK(!animation.capture({}, {"b"}));
}

TEST(change_animation_key_removed_and_inserted_moves) {
  ChangeAnimation animation;
  animation.capture({"a", "b"}, {"b", "c"});
  animation.recordPosition("a", {0, 0});
  animation.recordPosition("b", {0, 50});
  CHECK(animation.removedPosition("a").has_value());
  CHECK(!animation.removedPosition("b").has_value());

  // Across two changes: removed by one, inserted by the next.
  ChangeAnimation split;
  split.capture({"x"}, {});
  split.recordPosition("x", {0, 10});
  split.capture({}, {"x"});
  CHECK(!split.removedPosition("x").has_value());
  std::vector<ChangeStep> steps = split.run({"x"}, {{0, 30}});
  CHECK_EQ(steps.size(), std::size_t{1});
  CHECK(steps[0].kind == ChangeStepKind::Move);
  CHECK_NEAR(steps[0].fromY, -20.0, 1e-9);

  // Inserted by one change and removed by the next: it never showed and does nothing.
  ChangeAnimation brief;
  brief.capture({}, {"y"});
  brief.capture({"y"}, {});
  CHECK(!brief.removedPosition("y").has_value());
  CHECK(brief.run({}, {}).empty());
}

TEST(change_animation_run_slides_inserts_and_carries) {
  ChangeAnimation animation;
  animation.capture({"gone"}, {"new"});
  animation.recordPosition("top", {0, 0});
  animation.recordPosition("gone", {0, 40});
  animation.recordPosition("below", {0, 80});
  /*
   * After the layout: top stays, new sits where gone was, below moved down 10, then a row that
   * was off screen.
   */
  std::vector<ChangeStep> steps = animation.run(
    {"top", "new", "below", "offscreen"},
    {{0, 0}, {0, 40}, {0, 90}, {0, 140}});
  CHECK_EQ(steps.size(), std::size_t{4});
  CHECK(steps[0].kind == ChangeStepKind::Move);
  CHECK_NEAR(steps[0].fromY, 0.0, 1e-9);
  CHECK(steps[1].kind == ChangeStepKind::Insert);
  CHECK(steps[2].kind == ChangeStepKind::Move);
  CHECK_NEAR(steps[2].fromY, -10.0, 1e-9);
  CHECK(steps[3].kind == ChangeStepKind::Carry);
  CHECK_NEAR(steps[3].fromY, -10.0, 1e-9);

  // The run ends the animation.
  CHECK(!animation.isPending());
  CHECK(!animation.removedPosition("gone").has_value());
  CHECK(animation.run({"top"}, {{0, 0}}).empty());
}

TEST(change_animation_removed_row_fades_out_where_it_was) {
  ChangeAnimation animation;
  animation.capture({"gone"}, {});
  animation.recordPosition("gone", {12, 40});
  std::optional<ScreenPoint> position = animation.removedPosition("gone");
  CHECK(position.has_value());
  CHECK_NEAR(position->x, 12.0, 1e-9);
  CHECK_NEAR(position->y, 40.0, 1e-9);
  // A removed row that was not on screen has nowhere to fade out.
  animation.capture({"far"}, {});
  CHECK(!animation.removedPosition("far").has_value());
}
