#pragma once

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/host/LiveScroll.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace azimgd::shadowlist {

/*
 * A state update a host sends. It patches the newest committed state instead of copying
 * the mounted one, so a core change that is committed but not mounted yet is never undone.
 * The report and offsetEnabled are always set, the rest only by the update that owns it.
 */
struct ScrollPatch {
  LiveScroll::Report report;
  bool offsetEnabled = false;
  bool hasCommand = false;
  double commandIndex = 0.0;
  double commandSequence = 0.0;
  double commandViewPosition = 0.0;
  bool hasStartReachedEnabled = false;
  bool startReachedEnabled = true;
  bool hasEndReachedEnabled = false;
  bool endReachedEnabled = true;
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
  OffsetBand band;
};

/*
 * What the view is doing while a state mounts. Offsets are in the host's units.
 */
struct ViewMotion {
  double offsetX = 0.0;
  double offsetY = 0.0;
  /*
   * Where a shifted correction starts, usually the offset. iOS passes the offset from before
   * the content size write, which pulls a bounce back to the edge.
   */
  double shiftFromX = 0.0;
  double shiftFromY = 0.0;
  // The scroll range along the scroll axis, insets included.
  double minOffset = 0.0;
  double maxOffset = 0.0;
  bool touching = false;
  // A finger, momentum or an animation is moving the view.
  bool moving = false;
  // A row drag owns the offset, so corrections wait.
  bool ownsOffset = false;
  // An iOS scroll to top waiting to jump to jumpOffset along the scroll axis.
  bool jumpPending = false;
  double jumpOffset = 0.0;
};

/*
 * What to do with a mounted correction.
 */
struct MountAction {
  enum class Kind {
    None,
    // Write offsetX and offsetY into the view between ScrollSync::willWrite and didWrite.
    Write,
    // Move the waiting scroll to top jump to the offset instead of the view.
    RetargetJump,
  };
  Kind kind = Kind::None;
  double offsetX = 0.0;
  double offsetY = 0.0;
  std::uint64_t token = 0;
  // The correction was added to the live offset instead of written as is.
  bool shifted = false;
  // A host that can keep a fling going across the write should, like Android's scrollToPreservingMomentum.
  bool preserveMomentum = false;
};

/*
 * One scroll callback from the view.
 */
struct ScrollFrame {
  double offsetX = 0.0;
  double offsetY = 0.0;
  double scrollPhase = SCROLL_PHASE_IDLE;
  // Pull to refresh, scroll to top and row drags lean on every frame, so each one commits.
  bool commitEveryFrame = false;
};

struct FrameReport {
  // Whether the frame must become a state update, which is then patch.
  bool needsCommit = false;
  // The user moved the view, as opposed to our own write or a content size clamp.
  bool userScrolled = false;
  ScrollPatch patch;
};

/*
 * The host half of keeping the scroll view and the core in step, shared by every platform.
 *
 * Every scroll frame goes into the list's live report, and only frames the core needs become
 * state updates. The core answers with offset corrections, each with a token. The host writes
 * a correction, or adds it to the live offset while the view moves, and echoes the token on
 * later reports so the core knows its own write by id instead of guessing by distance.
 * The platform only reads and writes the scroll view, converts units at the edge, and runs
 * gestures and animations.
 */
class ScrollSync final {
public:
  struct Options {
    // How close our own scroll must land to its target to count as reaching it.
    double landingTolerance = 0.0;
    /*
     * Report the exact offset the core asked for when an instant write landed on it. A view
     * that only holds whole pixels would otherwise echo a rounded value, and the core would
     * move its anchor by that fraction on every correction.
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

#pragma mark - Mount

  /*
   * A new state mounts. Call before the content size write, so a clamp it causes counts as ours.
   */
  void beginMount(const MountedScroll& state, std::shared_ptr<LiveScroll> liveScroll);

  /*
   * Set around the content size write. A shorter content clamps the offset and the view
   * reports that as a scroll, which is not the user.
   */
  void setApplyingContentSize(bool applying) {
    applyingContentSize_ = applying;
  }

  /*
   * What to do with the mounted state's correction, if any.
   */
  MountAction correction(const ViewMotion& view);

  /*
   * Right before a Write, after stopping momentum if asked. The write's scroll frame is then
   * our own move and echoes the token.
   */
  void willWrite(const MountAction& action);

  /*
   * After a Write. A write that moved nothing fires no scroll frame, so the arm must go or it
   * would swallow the next real user scroll.
   */
  void didWrite(bool moved);

  /*
   * Whether the mounted state hides rows and waits for a report built on it that no scroll
   * frame sent during this mount. Send livePatch then.
   */
  bool concealAckDue(bool jumpPending) const;

  void endMount() {
    mounting_ = false;
  }

#pragma mark - Scroll frames

  FrameReport onScroll(const ScrollFrame& frame);

  /*
   * Mark a scroll we start ourselves, like a snap or scrollToOffset, so its frames are not
   * the user. An animated one lasts until it lands on the target. A token goes back to the
   * core like a correction's.
   */
  void arm(double offsetX, double offsetY, bool animated, std::uint64_t token = 0, bool exact = false);

  /*
   * A finger took over, so a scroll of ours still waiting for its frame is the user's now.
   */
  void disarm();

  bool armed() const {
    return armed_;
  }

  bool armedAnimated() const {
    return armed_ && armedAnimated_;
  }

  /*
   * The host stopped a fling or animation. Forget any scroll of ours waiting for its frame.
   */
  void momentumStopped();

#pragma mark - Updates

  /*
   * A patch with the live offset and the last echoed token, like a scroll report, with the
   * given gesture state. It acknowledges the rows the mounted state hides.
   */
  ScrollPatch livePatch(double offsetX, double offsetY, bool userScrolled, double scrollPhase);

  // Same with the newest report's gesture state.
  ScrollPatch livePatch(double offsetX, double offsetY);

  /*
   * A patch from a report the host built itself, like a scroll to top jump target.
   */
  ScrollPatch reportPatch(const LiveScroll::Report& report);

  /*
   * The gesture and momentum ended. Returns the rest report to send, or nothing when neither
   * the mounted state nor our last report was a gesture.
   */
  std::optional<ScrollPatch> clearUserScrolled(double offsetX, double offsetY);

  /*
   * A scroll command. The sequence always goes past the last one, so the same index still
   * scrolls again. Pass momentumYielded when a fling or animation was stopped for it, which
   * makes the report idle.
   */
  ScrollPatch issueCommand(double index, double viewPosition, double offsetX, double offsetY, bool momentumYielded);

  /*
   * The user scroll flag and phase that later updates carry, the newest report's. Before the
   * first report they are the mounted state's.
   */
  bool currentUserScrolled() const;
  double currentScrollPhase() const;

  std::uint64_t echoedToken() const {
    return echoedToken_;
  }

  const MountedScroll& mounted() const {
    return mounted_;
  }

private:
  LiveScroll::Report writeReport(double offsetX, double offsetY, bool userScrolled, double scrollPhase,
    std::uint64_t token, double concealGenerationAck);
  ScrollPatch push(const LiveScroll::Report& report);
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
  // Set when a scroll frame commits during a mount, which acknowledges hidden rows.
  bool reportedDuringMount_ = false;

  /*
   * Our own pending move. The next scroll frame is ours wherever it lands, or for an animated
   * one every frame until it reaches the target, and it echoes the token.
   */
  bool armed_ = false;
  bool armedAnimated_ = false;
  bool armedExact_ = false;
  double armedX_ = 0.0;
  double armedY_ = 0.0;
  std::uint64_t armedToken_ = 0;

  /*
   * Token of the last correction reported back. Later reports keep sending it: updates merge,
   * so the next frame can replace the echo before the core sees it. Tokens are never reused,
   * so an old one matches nothing.
   */
  std::uint64_t echoedToken_ = 0;

  /*
   * The last correction added to the live offset and how much of it is applied. The core
   * resends the full correction each time it retargets, so only add what's left.
   */
  std::uint64_t shiftedToken_ = 0;
  double shiftedTokenDelta_ = 0.0;

  /*
   * Whether the last report was a gesture. The mounted state lags behind, so it can't tell
   * alone whether the rest report is still due. See clearUserScrolled.
   */
  bool publishedGesture_ = false;

  LiveScroll::Report lastLiveReport_;
  LiveScroll::Report lastPushedReport_;
  bool hasPushedReport_ = false;

  /*
   * The last scroll command, copied into every update. Updates build on the mounted state,
   * which may not have the command yet, and would otherwise overwrite it before the core sees
   * it. The sequence is 0 until the first command.
   */
  double commandIndex_ = -2.0;
  double commandSequence_ = 0.0;
  double commandViewPosition_ = 0.0;
};

}
