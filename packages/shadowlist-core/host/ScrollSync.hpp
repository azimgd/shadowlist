#pragma once

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/host/LiveScroll.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace azimgd::shadowlist {

/*
 * A state update a host sends. It patches the newest committed state instead of copying the
 * mounted one. A committed core change is never undone. The report and offsetEnabled are
 * always set, the rest only by the update that owns it.
 */
struct ScrollPatch {
  LiveScroll::Report report;
  bool offsetEnabled = false;
  bool hasCommand = false;
  double commandIndex = 0.0;
  double commandSequence = 0.0;
  double commandViewPosition = 0.0;
  double commandRowOffset = 0.0;
  bool commandAnimated = false;
  bool hasAnchorRequest = false;
  double anchorRequestSequence = 0.0;
  bool hasStartReachedEnabled = false;
  bool startReachedEnabled = true;
  bool hasEndReachedEnabled = false;
  bool endReachedEnabled = true;
};

/*
 * A scroll command from the host: the row at index placed at viewPosition and moved rowOffset
 * further along the scroll axis, or the end for SCROLL_TO_END_INDEX. An animated command first
 * animates to the core's estimate, then lands exactly with the same command.
 */
struct ScrollCommand {
  double index = 0.0;
  double viewPosition = 0.0;
  double rowOffset = 0.0;
  bool animated = false;
};

/*
 * The scroll fields of the list state the host mounted.
 */
struct MountedScroll {
  bool offsetEnabled = false;
  double offsetX = 0.0;
  double offsetY = 0.0;
  double baseX = 0.0;
  double baseY = 0.0;
  std::uint64_t commitToken = 0;
  bool userScrolled = false;
  double scrollPhase = SCROLL_PHASE_IDLE;
  double concealGeneration = 0.0;
  double concealGenerationAck = 0.0;
  double commandSequence = 0.0;

  /*
   * Where the core estimates the newest animated command lands, along the scroll axis, and
   * that command's sequence. 0 before the first one.
   */
  double animationSequence = 0.0;
  double animationOffset = 0.0;
  OffsetBand band;
};

/*
 * What the view is doing while a state mounts. Offsets are in the host's units.
 */
struct ViewMotion {
  double offsetX = 0.0;
  double offsetY = 0.0;

  /*
   * Where a shifted correction starts, usually the offset.
   */
  double shiftFromX = 0.0;
  double shiftFromY = 0.0;

  /*
   * The scroll range along the scroll axis, insets included.
   */
  double minOffset = 0.0;
  double maxOffset = 0.0;
  bool touching = false;

  /*
   * A finger, momentum or an animation is moving the view.
   */
  bool moving = false;

  /*
   * A row drag owns the offset. Corrections wait.
   */
  bool ownsOffset = false;

  /*
   * An iOS scroll to top waiting to jump to jumpOffset along the scroll axis.
   */
  bool jumpPending = false;
  double jumpOffset = 0.0;
};

/*
 * What to do with a mounted correction.
 */
struct MountAction {
  enum class Kind {
    None,
    /*
     * Write offsetX and offsetY into the view between ScrollSync::willWrite and didWrite.
     */
    Write,
    /*
     * Move the waiting scroll to top jump to the offset instead of the view.
     */
    RetargetJump,
    /*
     * Animate the view to offsetX and offsetY for an animated scroll command. Arm the move as
     * animated first. Call ScrollSync::land when it ends.
     */
    Animate,
  };
  Kind kind = Kind::None;
  double offsetX = 0.0;
  double offsetY = 0.0;
  std::uint64_t token = 0;

  /*
   * The correction was added to the live offset instead of written as is.
   */
  bool shifted = false;

  /*
   * A host that can keep a fling going across the write should, like Android's scrollToPreservingMomentum.
   */
  bool preserveMomentum = false;
};

/*
 * One scroll callback from the view.
 */
struct ScrollFrame {
  double offsetX = 0.0;
  double offsetY = 0.0;
  double scrollPhase = SCROLL_PHASE_IDLE;

  /*
   * Pull to refresh, scroll to top and row drags lean on every frame. Each one commits.
   */
  bool commitEveryFrame = false;
};

struct FrameReport {
  /*
   * Whether the frame must become a state update, which is then patch.
   */
  bool needsCommit = false;

  /*
   * The user moved the view, as opposed to our own write or a content size clamp.
   */
  bool userScrolled = false;

  /*
   * An animated move of ours reached its target on this frame.
   */
  bool landed = false;
  ScrollPatch patch;
};

/*
 * The host half of keeping the scroll view and the core in step, shared by every platform.
 * Scroll frames go into the live report and only the ones the core needs become state
 * updates. Corrections come back with a token the host echoes once it wrote them.
 */
class ScrollSync final {
public:
  struct Options {
    /*
     * How close our own scroll must land to its target to count as reaching it.
     */
    double landingTolerance = 0.0;

    /*
     * Report the exact offset the core asked for when an instant write landed on it.
     */
    bool exactEcho = false;
  };

  ScrollSync() = default;
  explicit ScrollSync(Options options) : options_(options) {}

  void setHorizontal(bool horizontal) {
    horizontal_ = horizontal;
  }

  /*
   * Forget everything, for a view recycled into another list.
   */
  void reset();

  /*
   * A new state mounts. Call before the content size write. A clamp it causes then counts as ours.
   */
  void beginMount(const MountedScroll& state, std::shared_ptr<LiveScroll> liveScroll);

  /*
   * Set around the content size write. The clamp it causes is not taken for the user.
   */
  void setApplyingContentSize(bool applying) {
    applyingContentSize_ = applying;
  }

  /*
   * What to do with the mounted state's correction, if any.
   */
  MountAction correction(const ViewMotion& view);

  /*
   * Right before a Write. Its scroll frame is our own move and echoes the token.
   */
  void willWrite(const MountAction& action);

  /*
   * After a Write. A write that moved nothing fires no scroll frame and disarms.
   */
  void didWrite(bool moved);

  /*
   * Whether the mounted state hides rows and no scroll frame acknowledged them during this
   * mount. Send livePatch then.
   */
  bool concealAckDue(bool jumpPending) const;

  void endMount() {
    mounting_ = false;
  }

  FrameReport onScroll(const ScrollFrame& frame);

  /*
   * Mark a scroll we start ourselves, like a snap or scrollToOffset. Its frames are not the
   * user. An animated one lasts until it lands on the target.
   */
  void arm(double offsetX, double offsetY, bool animated, std::uint64_t token = 0, bool exact = false);

  /*
   * A finger took over. A scroll of ours still waiting for its frame is the user's now.
   */
  void disarm();

  bool isArmed() const {
    return armed_;
  }

  bool isArmedAnimated() const {
    return armed_ && armedAnimated_;
  }

  /*
   * The host stopped a fling or animation. Forget any scroll of ours waiting for its frame.
   */
  void momentumStopped();

  /*
   * A patch with the live offset and the echoed token, like a scroll report, with the given
   * gesture state. It acknowledges the rows the mounted state hides.
   */
  ScrollPatch livePatch(double offsetX, double offsetY, bool userScrolled, double scrollPhase);

  /*
   * Same with the newest report's gesture state.
   */
  ScrollPatch livePatch(double offsetX, double offsetY);

  /*
   * A patch from a report the host built itself, like a scroll to top jump target.
   */
  ScrollPatch reportPatch(const LiveScroll::Report& report);

  /*
   * The gesture and momentum ended. Returns the rest report to send, or nothing when neither
   * the mounted state nor our newest report was a gesture.
   */
  std::optional<ScrollPatch> clearUserScrolled(double offsetX, double offsetY);

  /*
   * A scroll command. The sequence always goes past the previous one. The same index still
   * scrolls again. momentumYielded makes the report idle after a fling was stopped for it.
   */
  ScrollPatch issueCommand(const ScrollCommand& command, double offsetX, double offsetY, bool momentumYielded);

  /*
   * The animation of an animated command ended. Returns the same command without the animation,
   * which lands it exactly, or nothing when no animated command waits.
   */
  std::optional<ScrollPatch> land(double offsetX, double offsetY);

  /*
   * Whether an animated command is on its way and waits for land.
   */
  bool isLanding() const {
    return landing_;
  }

  /*
   * Ask the core for the anchor at the live offset. The sequence goes past the previous one.
   */
  ScrollPatch requestAnchor(double offsetX, double offsetY);

  /*
   * The user scroll flag and phase that later updates carry, the newest report's. Before the
   * first report they are the mounted state's.
   */
  bool isCurrentUserScrolled() const;
  double getCurrentScrollPhase() const;

  std::uint64_t getEchoedToken() const {
    return echoedToken_;
  }

private:
  LiveScroll::Report writeReport(
    double offsetX,
    double offsetY,
    bool userScrolled,
    double scrollPhase,
    std::uint64_t token,
    double concealGenerationAck);
  ScrollPatch push(const LiveScroll::Report& report);

  /*
   * Our write is done, echoed or moved nothing. Unlike a finger, it leaves an animated command
   * that still waits for its estimate alone.
   */
  void release();
  bool reportNeedsCommit(const LiveScroll::Report& report, bool commitEveryFrame) const;

  double along(double x, double y) const {
    return horizontal_ ? x : y;
  }

  Options options_;
  bool horizontal_ = false;

  std::shared_ptr<LiveScroll> liveScroll_;
  MountedScroll mounted_;
  bool hasMounted_ = false;
  bool mounting_ = false;
  bool applyingContentSize_ = false;

  /*
   * Set when a scroll frame commits during a mount, which acknowledges hidden rows.
   */
  bool reportedDuringMount_ = false;

  /*
   * Our own pending move, which the next scroll frame or an animation's frames echo.
   */
  bool armed_ = false;
  bool armedAnimated_ = false;
  bool armedExact_ = false;
  double armedX_ = 0.0;
  double armedY_ = 0.0;
  std::uint64_t armedToken_ = 0;

  /*
   * Token of the newest correction reported back. Later reports keep sending it, since updates
   * merge and the next frame could replace the echo before the core sees it.
   */
  std::uint64_t echoedToken_ = 0;

  /*
   * The newest correction added to the live offset and how much of it is applied.
   */
  std::uint64_t shiftedToken_ = 0;
  double shiftedTokenDelta_ = 0.0;

  /*
   * Whether the newest report was a gesture, see clearUserScrolled.
   */
  bool publishedGesture_ = false;

  LiveScroll::Report liveReport_;
  LiveScroll::Report pushedReport_;
  bool hasPushedReport_ = false;

  /*
   * The newest scroll command, copied into every update so one built on a mounted state
   * without it can't overwrite it. The sequence is 0 until the first command.
   */
  double commandIndex_ = -2.0;
  double commandSequence_ = 0.0;
  double commandViewPosition_ = 0.0;
  double commandRowOffset_ = 0.0;
  bool commandAnimated_ = false;

  /*
   * The newest animated command whose animation started or was given up. An animation starts
   * only for a newer one. landing_ is set from the start until land or a finger takes over.
   */
  double animatedSequence_ = 0.0;
  bool landing_ = false;

  double anchorRequestSequence_ = 0.0;
};

}
