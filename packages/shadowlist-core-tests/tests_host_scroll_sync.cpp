/*
 * Host scroll protocol tests. ScrollSync is what both platform views run on every scroll
 * frame and every mount: which frames commit, how a mounted correction is written or shifted,
 * and how its token is echoed back.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/ScrollSync.hpp>

#include <cmath>
#include <memory>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

MountedScroll restingState(double offsetY) {
  MountedScroll state;
  state.offsetY = offsetY;
  state.band.low = offsetY - 100.0;
  state.band.high = offsetY + 100.0;
  return state;
}

MountedScroll correctionState(double offsetY, double baseY, std::uint64_t token) {
  MountedScroll state;
  state.offsetEnabled = true;
  state.offsetY = offsetY;
  state.baseY = baseY;
  state.commitToken = token;
  return state;
}

ViewMotion viewAt(double offsetY, double maxOffset = 5000.0) {
  ViewMotion view;
  view.offsetY = offsetY;
  view.shiftFromY = offsetY;
  view.maxOffset = maxOffset;
  return view;
}

ScrollFrame frameAt(double offsetY, double scrollPhase = SCROLL_PHASE_IDLE) {
  ScrollFrame frame;
  frame.offsetY = offsetY;
  frame.scrollPhase = scrollPhase;
  return frame;
}

/*
 * Mount a state, run its correction and write it into a view at offsetY, returning where
 * the view ends up. The write's scroll frame follows right away, like on both platforms.
 */
double mountAndWrite(
  ScrollSync& sync,
  const MountedScroll& state,
  const std::shared_ptr<LiveScroll>& live,
  const ViewMotion& view,
  FrameReport* written = nullptr) {
  sync.beginMount(state, live);
  MountAction action = sync.correction(view);
  double offset = view.offsetY;
  if (action.kind == MountAction::Kind::Write) {
    sync.willWrite(action);
    bool moved = std::fabs(action.offsetY - offset) >= 0.01;
    if (moved) {
      FrameReport report = sync.onScroll(frameAt(action.offsetY));
      if (written != nullptr) {
        *written = report;
      }
    }
    sync.didWrite(moved);
    offset = action.offsetY;
  }
  sync.endMount();
  return offset;
}

}

TEST(scroll_sync_first_frame_commits_then_band_holds) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  sync.beginMount(restingState(0.0), live);
  sync.endMount();

  FrameReport first = sync.onScroll(frameAt(10.0, SCROLL_PHASE_DRAGGING));
  CHECK(first.needsCommit);
  CHECK(first.userScrolled);
  CHECK(first.patch.report.userScrolled);
  CHECK_EQ(first.patch.report.sequence, static_cast<std::uint64_t>(1));

  // Same gesture inside the band only goes into the live report.
  FrameReport inside = sync.onScroll(frameAt(40.0, SCROLL_PHASE_DRAGGING));
  CHECK(!inside.needsCommit);
  CHECK_EQ(live->read().offsetY, 40.0);
  CHECK_EQ(live->read().sequence, static_cast<std::uint64_t>(2));

  // Leaving the band commits.
  FrameReport outside = sync.onScroll(frameAt(140.0, SCROLL_PHASE_DRAGGING));
  CHECK(outside.needsCommit);

  // A phase change commits even inside the band.
  sync.beginMount(restingState(140.0), live);
  sync.endMount();
  sync.onScroll(frameAt(141.0, SCROLL_PHASE_DRAGGING));
  CHECK(sync.onScroll(frameAt(142.0, SCROLL_PHASE_SETTLING)).needsCommit);
}

TEST(scroll_sync_without_live_report_commits_every_frame) {
  ScrollSync sync;
  sync.beginMount(restingState(0.0), nullptr);
  sync.endMount();
  CHECK(sync.onScroll(frameAt(5.0)).needsCommit);
  CHECK(sync.onScroll(frameAt(6.0)).needsCommit);
}

TEST(scroll_sync_idle_correction_writes_and_echoes_its_token) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  FrameReport written;
  double offset = mountAndWrite(sync, correctionState(300.0, 200.0, 7), live, viewAt(200.0), &written);
  CHECK_EQ(offset, 300.0);
  CHECK(written.needsCommit);
  CHECK(!written.userScrolled);
  CHECK_EQ(written.patch.report.commitToken, static_cast<std::uint64_t>(7));
  CHECK_EQ(sync.getEchoedToken(), static_cast<std::uint64_t>(7));

  // Later reports keep sending the token, and a user frame is the user again.
  sync.beginMount(restingState(300.0), live);
  sync.endMount();
  FrameReport later = sync.onScroll(frameAt(310.0, SCROLL_PHASE_DRAGGING));
  CHECK(later.userScrolled);
  CHECK_EQ(later.patch.report.commitToken, static_cast<std::uint64_t>(7));
}

TEST(scroll_sync_moving_view_shifts_by_the_unapplied_part) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  ViewMotion moving = viewAt(260.0);
  moving.moving = true;
  // The core corrected +100 from 200, but the fling already moved the view to 260.
  double offset = mountAndWrite(sync, correctionState(300.0, 200.0, 9), live, moving);
  CHECK_EQ(offset, 360.0);

  // The core retargets the same token to +130 from the same base. Only 30 more is added.
  ViewMotion further = viewAt(380.0);
  further.moving = true;
  offset = mountAndWrite(sync, correctionState(330.0, 200.0, 9), live, further);
  CHECK_EQ(offset, 410.0);

  // Once the motion stops, a token already shifted keeps shifting instead of jumping.
  offset = mountAndWrite(sync, correctionState(340.0, 200.0, 9), live, viewAt(420.0));
  CHECK_EQ(offset, 430.0);
}

/*
 * Android mounts the same state again after the write moved the view. scrollToEnd landed one
 * correction short when that second mount added it a second time.
 */
TEST(scroll_sync_written_correction_is_not_shifted_again) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  double offset = mountAndWrite(sync, correctionState(92.0, 100.0, 3), live, viewAt(100.0));
  CHECK_EQ(offset, 92.0);

  ViewMotion moving = viewAt(92.0);
  moving.moving = true;
  offset = mountAndWrite(sync, correctionState(92.0, 100.0, 3), live, moving);
  CHECK_EQ(offset, 92.0);

  // The same token retargeted from the same base moves only by the difference.
  offset = mountAndWrite(sync, correctionState(110.0, 100.0, 3), live, viewAt(92.0));
  CHECK_EQ(offset, 110.0);
}

TEST(scroll_sync_gesture_correction_shifts_after_the_motion_stops) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  MountedScroll state = correctionState(500.0, 450.0, 3);
  state.userScrolled = true;
  double offset = mountAndWrite(sync, state, live, viewAt(470.0));
  CHECK_EQ(offset, 520.0);
}

TEST(scroll_sync_shift_is_clamped_to_the_scroll_range) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  ViewMotion moving = viewAt(900.0, 950.0);
  moving.moving = true;
  CHECK_EQ(mountAndWrite(sync, correctionState(400.0, 300.0, 4), live, moving), 950.0);

  ScrollSync top;
  ViewMotion bounce = viewAt(20.0, 950.0);
  bounce.moving = true;
  bounce.minOffset = -60.0;
  CHECK_EQ(mountAndWrite(top, correctionState(0.0, 200.0, 5), live, bounce), -60.0);
}

TEST(scroll_sync_shift_starts_from_the_bounced_offset) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  // The content size write pulled a bounce at -40 back to 0. The core measured from -40.
  ViewMotion view = viewAt(0.0);
  view.shiftFromY = -40.0;
  view.moving = true;
  view.minOffset = 0.0;
  CHECK_EQ(mountAndWrite(sync, correctionState(160.0, 0.0, 6), live, view), 120.0);
}

TEST(scroll_sync_content_size_clamp_is_not_a_user_scroll) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  sync.beginMount(restingState(800.0), live);
  sync.onScroll(frameAt(800.0));
  sync.setApplyingContentSize(true);
  FrameReport clamp = sync.onScroll(frameAt(600.0));
  sync.setApplyingContentSize(false);
  sync.endMount();
  CHECK(!clamp.userScrolled);
  CHECK(clamp.needsCommit);
  CHECK(!clamp.patch.report.userScrolled);
}

TEST(scroll_sync_write_that_moved_nothing_disarms) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  mountAndWrite(sync, correctionState(300.0, 300.0, 2), live, viewAt(300.0));
  CHECK(!sync.isArmed());
  sync.beginMount(restingState(300.0), live);
  sync.endMount();
  CHECK(sync.onScroll(frameAt(320.0, SCROLL_PHASE_DRAGGING)).userScrolled);
}

TEST(scroll_sync_exact_echo_reports_the_requested_offset) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync::Options options;
  options.landingTolerance = 0.8;
  options.exactEcho = true;
  ScrollSync sync(options);
  sync.beginMount(correctionState(100.3, 50.0, 8), live);
  MountAction action = sync.correction(viewAt(50.0));
  sync.willWrite(action);
  // Whole pixels put the view a fraction off.
  FrameReport landed = sync.onScroll(frameAt(100.19));
  sync.didWrite(true);
  sync.endMount();
  CHECK_EQ(landed.patch.report.offsetY, 100.3);

  // A write clamped far from its target reports where the view really is.
  ScrollSync clamped(options);
  clamped.beginMount(correctionState(900.0, 50.0, 10), live);
  clamped.willWrite(clamped.correction(viewAt(50.0)));
  FrameReport real = clamped.onScroll(frameAt(700.0));
  CHECK_EQ(real.patch.report.offsetY, 700.0);
}

TEST(scroll_sync_animated_scroll_is_ours_until_it_lands) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync::Options options;
  options.landingTolerance = 1.0;
  ScrollSync sync(options);
  sync.beginMount(restingState(0.0), live);
  sync.endMount();
  sync.arm(0.0, 400.0, true);
  FrameReport middle = sync.onScroll(frameAt(200.0));
  CHECK(!middle.userScrolled);
  CHECK(middle.needsCommit);
  CHECK(sync.isArmed());
  FrameReport end = sync.onScroll(frameAt(399.5));
  CHECK(!end.userScrolled);
  CHECK(!sync.isArmed());
  CHECK(sync.onScroll(frameAt(380.0, SCROLL_PHASE_DRAGGING)).userScrolled);
}

TEST(scroll_sync_clear_user_scrolled_only_after_a_gesture) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  sync.beginMount(restingState(0.0), live);
  sync.endMount();
  CHECK(!sync.clearUserScrolled(0.0, 0.0).has_value());

  sync.onScroll(frameAt(50.0, SCROLL_PHASE_DRAGGING));
  auto rest = sync.clearUserScrolled(50.0, 0.0);
  CHECK(rest.has_value());
  CHECK(!rest->report.userScrolled);
  CHECK_EQ(rest->report.scrollPhase, SCROLL_PHASE_IDLE);
  CHECK(!sync.clearUserScrolled(50.0, 0.0).has_value());

  // A mounted gesture state still needs the rest report even when our newest report was idle.
  MountedScroll mounted = restingState(50.0);
  mounted.userScrolled = true;
  sync.beginMount(mounted, live);
  sync.endMount();
  CHECK(sync.clearUserScrolled(0.0, 50.0).has_value());
}

TEST(scroll_sync_commands_sequence_past_the_mounted_one) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  MountedScroll mounted = restingState(0.0);
  mounted.commandSequence = 5;
  sync.beginMount(mounted, live);
  sync.endMount();
  sync.onScroll(frameAt(10.0, SCROLL_PHASE_SETTLING));

  ScrollPatch command = sync.issueCommand({40.0, 0.5, 0.0, false}, 0.0, 10.0, true);
  CHECK(command.offsetEnabled);
  CHECK(command.hasCommand);
  CHECK_EQ(command.commandSequence, 6.0);
  CHECK_EQ(command.commandIndex, 40.0);
  CHECK_EQ(command.commandViewPosition, 0.5);
  CHECK_EQ(command.report.scrollPhase, SCROLL_PHASE_IDLE);

  // Every later update carries the command until the core has it.
  ScrollPatch report = sync.livePatch(0.0, 12.0);
  CHECK(report.hasCommand);
  CHECK_EQ(report.commandSequence, 6.0);
  CHECK(!report.offsetEnabled);

  // The mounted state is behind. The next command still goes past ours.
  CHECK_EQ(sync.issueCommand({-3.0, 0.0, 0.0, false}, 0.0, 12.0, false).commandSequence, 7.0);
}

TEST(scroll_sync_correction_moves_a_waiting_scroll_to_top_jump) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  ViewMotion view = viewAt(3000.0);
  view.moving = true;
  view.jumpPending = true;
  view.jumpOffset = 800.0;
  sync.beginMount(correctionState(950.0, 800.0, 21), live);
  MountAction action = sync.correction(view);
  sync.endMount();
  CHECK(action.kind == MountAction::Kind::RetargetJump);
  CHECK_EQ(action.offsetY, 950.0);
  CHECK_EQ(action.commitToken, static_cast<std::uint64_t>(21));

  // The jump applies the whole correction. A resend of the token only adds what's new.

  ViewMotion landed = viewAt(950.0);
  landed.moving = true;
  CHECK_EQ(mountAndWrite(sync, correctionState(960.0, 800.0, 21), live, landed), 960.0);
}

TEST(scroll_sync_drag_owning_the_offset_skips_corrections) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  ViewMotion view = viewAt(100.0);
  view.ownsOffset = true;
  sync.beginMount(correctionState(300.0, 100.0, 4), live);
  CHECK(sync.correction(view).kind == MountAction::Kind::None);
  sync.endMount();
}

TEST(scroll_sync_conceal_ack_is_due_only_without_a_report) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  MountedScroll state = correctionState(200.0, 200.0, 5);
  state.concealGeneration = 3.0;
  sync.beginMount(state, live);
  sync.correction(viewAt(200.0));
  CHECK(sync.concealAckDue(false));
  CHECK(!sync.concealAckDue(true));
  sync.endMount();

  MountedScroll moved = correctionState(260.0, 200.0, 6);
  moved.concealGeneration = 4.0;
  ScrollSync reporting;
  reporting.beginMount(moved, live);
  MountAction action = reporting.correction(viewAt(200.0));
  reporting.willWrite(action);
  FrameReport report = reporting.onScroll(frameAt(260.0));
  CHECK_EQ(report.patch.report.concealGenerationAck, 4.0);
  CHECK(!reporting.concealAckDue(false));
  reporting.endMount();
}

TEST(scroll_sync_live_patch_uses_the_newest_gesture_state) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  MountedScroll mounted = restingState(0.0);
  mounted.scrollPhase = SCROLL_PHASE_SETTLING;
  sync.beginMount(mounted, live);
  sync.endMount();
  // Before any report the mounted state's phase counts.
  CHECK_EQ(sync.livePatch(0.0, 0.0).report.scrollPhase, SCROLL_PHASE_SETTLING);
  sync.onScroll(frameAt(5.0, SCROLL_PHASE_DRAGGING));
  CHECK_EQ(sync.livePatch(0.0, 5.0).report.scrollPhase, SCROLL_PHASE_DRAGGING);
  CHECK(sync.livePatch(0.0, 5.0).report.userScrolled);
}

TEST(live_scroll_newer_report_rules) {
  LiveScroll live;
  LiveScroll::Report report;
  CHECK(!live.newerReport(0.0, false, report));
  LiveScroll::Report written;
  written.offsetY = 42.0;
  live.write(written);
  CHECK(live.newerReport(0.0, false, report));
  CHECK_EQ(report.offsetY, 42.0);
  CHECK(!live.newerReport(1.0, false, report));
  // A state with the core's own offset to apply is left alone.
  CHECK(!live.newerReport(0.0, true, report));
}

TEST(live_scroll_registry_finds_live_lists_only) {
  auto live = std::make_shared<LiveScroll>();
  LiveScroll::registerHandle(live);
  CHECK(LiveScroll::find(live->getHandle()) == live);
  std::int64_t handle = live->getHandle();
  live.reset();
  CHECK(LiveScroll::find(handle) == nullptr);
}

/*
 * An animated command moves nothing at first. The view animates to the core's estimate, then
 * the same command runs without the animation and lands exactly.
 */
TEST(scroll_sync_animated_command_animates_to_the_estimate_then_lands) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  sync.beginMount(restingState(0.0), live);
  sync.endMount();

  ScrollPatch command = sync.issueCommand({40.0, 0.5, -20.0, true}, 0.0, 0.0, false);
  CHECK(!command.offsetEnabled);
  CHECK(command.commandAnimated);
  CHECK_EQ(command.commandViewOffset, -20.0);
  CHECK_EQ(command.commandSequence, 1.0);

  // The state with the estimate mounts. The view animates there once.
  MountedScroll estimated = restingState(0.0);
  estimated.commandSequence = 1.0;
  estimated.animationSequence = 1.0;
  estimated.animationOffset = 3200.0;
  sync.beginMount(estimated, live);
  MountAction action = sync.correction(viewAt(0.0));
  sync.endMount();
  CHECK(action.kind == MountAction::Kind::Animate);
  CHECK_EQ(action.offsetY, 3200.0);
  CHECK(sync.isLanding());
  sync.beginMount(estimated, live);
  CHECK(sync.correction(viewAt(0.0)).kind == MountAction::Kind::None);
  sync.endMount();

  // The animation's frames are ours, and the last one lands it.
  sync.arm(0.0, 3200.0, true);
  FrameReport middle = sync.onScroll(frameAt(1600.0));
  CHECK(!middle.userScrolled);
  CHECK(!middle.landed);
  FrameReport last = sync.onScroll(frameAt(3200.0));
  CHECK(last.landed);

  auto landing = sync.land(0.0, 3200.0);
  CHECK(landing.has_value());
  CHECK(landing->offsetEnabled);
  CHECK(!landing->commandAnimated);
  CHECK_EQ(landing->commandIndex, 40.0);
  CHECK_EQ(landing->commandViewPosition, 0.5);
  CHECK_EQ(landing->commandViewOffset, -20.0);
  CHECK_EQ(landing->commandSequence, 2.0);
  CHECK(!sync.isLanding());
  CHECK(!sync.land(0.0, 3200.0).has_value());
}

/*
 * A finger on the list gives an animated command up, before or after its animation started.
 */
TEST(scroll_sync_a_finger_gives_an_animated_command_up) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  sync.beginMount(restingState(0.0), live);
  sync.endMount();
  sync.issueCommand({40.0, 0.0, 0.0, true}, 0.0, 0.0, false);
  sync.disarm();

  MountedScroll estimated = restingState(0.0);
  estimated.commandSequence = 1.0;
  estimated.animationSequence = 1.0;
  estimated.animationOffset = 3200.0;
  sync.beginMount(estimated, live);
  CHECK(sync.correction(viewAt(0.0)).kind == MountAction::Kind::None);
  sync.endMount();
  CHECK(!sync.isLanding());
  CHECK(!sync.land(0.0, 0.0).has_value());
}

/*
 * While an animated command is on its way, a correction waits. Writing it would stop the
 * animation, and the landing command replaces it.
 */
TEST(scroll_sync_a_correction_waits_for_an_animated_command) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  sync.beginMount(restingState(0.0), live);
  sync.endMount();
  sync.issueCommand({40.0, 0.0, 0.0, true}, 0.0, 0.0, false);
  MountedScroll estimated = restingState(0.0);
  estimated.commandSequence = 1.0;
  estimated.animationSequence = 1.0;
  estimated.animationOffset = 3200.0;
  sync.beginMount(estimated, live);
  CHECK(sync.correction(viewAt(0.0)).kind == MountAction::Kind::Animate);
  sync.endMount();
  sync.arm(0.0, 3200.0, true);

  MountedScroll corrected = correctionState(1400.0, 1300.0, 9);
  corrected.commandSequence = 1.0;
  corrected.animationSequence = 1.0;
  corrected.animationOffset = 3200.0;
  sync.beginMount(corrected, live);
  CHECK(sync.correction(viewAt(1300.0)).kind == MountAction::Kind::None);
  sync.endMount();
}

/*
 * A correction written while an animated command waits for its estimate is no finger. The
 * estimate that mounts after it still animates.
 */
TEST(scroll_sync_a_correction_write_keeps_a_waiting_animated_command) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  sync.beginMount(restingState(0.0), live);
  sync.endMount();
  sync.issueCommand({40.0, 0.5, 0.0, true}, 0.0, 0.0, false);

  // A correction from a commit made before the command mounts first and is written.
  CHECK_EQ(mountAndWrite(sync, correctionState(140.0, 100.0, 5), live, viewAt(100.0)), 140.0);
  // A write that moved nothing is let go too.
  CHECK_EQ(mountAndWrite(sync, correctionState(140.0, 140.0, 6), live, viewAt(140.0)), 140.0);

  MountedScroll estimated = restingState(140.0);
  estimated.commandSequence = 1.0;
  estimated.animationSequence = 1.0;
  estimated.animationOffset = 3200.0;
  sync.beginMount(estimated, live);
  MountAction action = sync.correction(viewAt(140.0));
  sync.endMount();
  CHECK(action.kind == MountAction::Kind::Animate);
  CHECK_EQ(action.offsetY, 3200.0);
  CHECK(sync.isLanding());
}

TEST(scroll_sync_anchor_requests_sequence_up_and_ride_along) {
  auto live = std::make_shared<LiveScroll>();
  ScrollSync sync;
  sync.beginMount(restingState(0.0), live);
  sync.endMount();
  CHECK(!sync.livePatch(0.0, 10.0).hasAnchorRequest);
  ScrollPatch first = sync.requestAnchor(0.0, 10.0);
  CHECK(first.hasAnchorRequest);
  CHECK_EQ(first.anchorRequestSequence, 1.0);
  CHECK_EQ(sync.requestAnchor(0.0, 10.0).anchorRequestSequence, 2.0);
  CHECK_EQ(sync.livePatch(0.0, 12.0).anchorRequestSequence, 2.0);
}
