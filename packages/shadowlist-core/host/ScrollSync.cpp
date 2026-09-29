#include <shadowlist-core/host/ScrollSync.hpp>

#include <algorithm>
#include <cmath>

namespace azimgd::shadowlist {

void ScrollSync::reset() {
  Options options = options_;
  bool horizontal = horizontal_;
  *this = ScrollSync(options);
  horizontal_ = horizontal;
}

#pragma mark - Mount

void ScrollSync::beginMount(const MountedScroll& state, std::shared_ptr<LiveScroll> liveScroll) {
  mounted_ = state;
  hasMounted_ = true;
  liveScroll_ = std::move(liveScroll);
  mounting_ = true;
  reportedDuringMount_ = false;
}

MountAction ScrollSync::correction(const ViewMotion& view) {
  MountAction action;
  const MountedScroll& state = mounted_;
  std::uint64_t token = state.commitToken;
  double base = along(state.baseX, state.baseY);
  double target = along(state.offsetX, state.offsetY);

  /*
   * A correction made against a waiting scroll to top jump moves the jump target instead of
   * the view. The jump applies the whole correction when it lands, and the core may send the
   * same token again with the full amount, so remember it as applied.
   */
  if (view.jumpPending && state.offsetEnabled && std::fabs(base - view.jumpOffset) < 0.5) {
    action.kind = MountAction::Kind::RetargetJump;
    action.offsetX = state.offsetX;
    action.offsetY = state.offsetY;
    action.token = token;
    if (token != 0) {
      shiftedToken_ = token;
      shiftedTokenDelta_ = target - base;
    }
    return action;
  }

  // While a row is dragged or dropping the drag owns the offset.
  if (!state.offsetEnabled || view.ownsOffset) {
    return action;
  }

  /*
   * A scroll command issued in C++, like a ShadowListNative scrollToIndex, reaches the core in
   * a commit, not through the view. Stop momentum when it mounts and write the offset.
   * A finger on the list wins, and the core lets the drag cancel the command.
   */
  std::uint64_t yieldToken = state.momentumYieldToken;
  bool scrollCommand = yieldToken != 0 && token == yieldToken && !view.touching;
  if (scrollCommand && yieldToken != yieldedToken_) {
    yieldedToken_ = yieldToken;
    action.stopMomentum = true;
  }

  double offsetX = state.offsetX;
  double offsetY = state.offsetY;

  /*
   * While the view moves it has gone on by the time a correction mounts, and writing the fixed
   * offset would undo that. So add the correction to the live offset instead, clamped to the
   * scroll range. Keep shifting for corrections made during a gesture, or ones already shifted,
   * even after the motion stops. The core accepts those by their echo and never moves the view
   * to an exact target, so lost movement would never come back.
   */
  bool continuesShiftedCorrection = token != 0 && token == shiftedToken_;
  bool computedDuringGesture = token != 0 && (state.userScrolled || state.scrollPhase != SCROLL_PHASE_IDLE);
  bool shift = !scrollCommand && (view.moving || continuesShiftedCorrection || computedDuringGesture);
  if (shift) {
    double delta = target - base;
    /*
     * The core resends the full correction with each new report until the token is echoed.
     * Shift only by what this token has not moved yet, or a prepend during a fling would
     * shift the view again each time.
     */
    if (token != 0) {
      double unapplied = token == shiftedToken_ ? delta - shiftedTokenDelta_ : delta;
      shiftedToken_ = token;
      shiftedTokenDelta_ = delta;
      delta = unapplied;
    }
    double shifted = std::min(std::max(along(view.shiftFromX, view.shiftFromY) + delta, view.minOffset),
      std::max(view.minOffset, view.maxOffset));
    offsetX = horizontal_ ? shifted : view.offsetX;
    offsetY = horizontal_ ? view.offsetY : shifted;
  }

  /*
   * Corrections with a token and shifted ones keep a fling going. A write with no token may
   * carry an offset a frame old, and rebuilding the fling from it would restart momentum from
   * the past on every frame. Our own animated scrolls use a plain write too.
   */
  action.preserveMomentum = !scrollCommand && (token != 0 || shift) && !armedAnimated();

  action.kind = MountAction::Kind::Write;
  action.offsetX = offsetX;
  action.offsetY = offsetY;
  action.token = token;
  action.shifted = shift;
  return action;
}

void ScrollSync::willWrite(const MountAction& action) {
  arm(action.offsetX, action.offsetY, false, action.token, !action.shifted && options_.exactEcho);
}

void ScrollSync::didWrite(bool moved) {
  if (!moved) {
    disarm();
  }
}

bool ScrollSync::concealAckDue(bool jumpPending) const {
  return mounted_.concealGeneration != 0.0 && mounted_.offsetEnabled && !reportedDuringMount_ && !jumpPending;
}

#pragma mark - Scroll frames

FrameReport ScrollSync::onScroll(const ScrollFrame& frame) {
  FrameReport result;
  double offsetX = frame.offsetX;
  double offsetY = frame.offsetY;

  /*
   * Tell a user scroll from our own move by who caused it. If we just wrote an offset, this
   * frame is ours wherever it landed and echoes the token. An animated scroll of ours is ours
   * on every frame until it reaches its target.
   */
  bool userScrolled = !applyingContentSize_;
  bool ourMove = false;
  if (armed_) {
    userScrolled = false;
    ourMove = true;
    bool landed = std::fabs(offsetX - armedX_) <= options_.landingTolerance &&
      std::fabs(offsetY - armedY_) <= options_.landingTolerance;
    if (armedAnimated_) {
      if (landed) {
        echoedToken_ = armedToken_;
        disarm();
      }
    } else {
      echoedToken_ = armedToken_;
      if (landed && armedExact_) {
        offsetX = armedX_;
        offsetY = armedY_;
      }
      disarm();
    }
  }

  LiveScroll::Report report =
    writeReport(offsetX, offsetY, userScrolled, frame.scrollPhase, echoedToken_, mounted_.concealGeneration);
  result.userScrolled = userScrolled;
  result.needsCommit = ourMove || reportNeedsCommit(report, frame.commitEveryFrame);
  if (result.needsCommit) {
    result.patch = push(report);
    if (mounting_) {
      reportedDuringMount_ = true;
    }
  }
  return result;
}

void ScrollSync::arm(double offsetX, double offsetY, bool animated, std::uint64_t token, bool exact) {
  armed_ = true;
  armedAnimated_ = animated;
  armedExact_ = exact;
  armedX_ = offsetX;
  armedY_ = offsetY;
  armedToken_ = token;
}

void ScrollSync::disarm() {
  armed_ = false;
  armedAnimated_ = false;
  armedExact_ = false;
  armedToken_ = 0;
}

void ScrollSync::momentumStopped() {
  disarm();
  publishedGesture_ = false;
}

/*
 * Whether a scroll frame must become a state update. Conservative on purpose: anything the
 * core reacts to other than the offset moving inside the band sends it.
 */
bool ScrollSync::reportNeedsCommit(const LiveScroll::Report& report, bool commitEveryFrame) const {
  if (!liveScroll_ || report.sequence == 0 || !hasPushedReport_) {
    return true;
  }
  // A correction just mounted, or rows hidden until a report acknowledges them.
  if (mounted_.offsetEnabled || mounted_.concealGeneration != 0.0) {
    return true;
  }
  // The gesture changed, a correction was echoed, or a hide was acknowledged.
  if (report.userScrolled != lastPushedReport_.userScrolled || report.scrollPhase != lastPushedReport_.scrollPhase ||
      report.commitToken != lastPushedReport_.commitToken ||
      report.concealGenerationAck != lastPushedReport_.concealGenerationAck) {
    return true;
  }
  // The view clamped the offset to a new content size, or a mount moved it.
  if (applyingContentSize_ || mounting_ || commitEveryFrame) {
    return true;
  }
  // The offset left the band the mounted layout pass published. An empty band never holds it.
  return !mounted_.band.contains(along(report.offsetX, report.offsetY));
}

#pragma mark - Updates

LiveScroll::Report ScrollSync::writeReport(double offsetX, double offsetY, bool userScrolled, double scrollPhase,
  std::uint64_t token, double concealGenerationAck) {
  LiveScroll::Report report;
  report.offsetX = offsetX;
  report.offsetY = offsetY;
  report.userScrolled = userScrolled;
  report.scrollPhase = scrollPhase;
  report.commitToken = static_cast<double>(token);
  report.concealGenerationAck = concealGenerationAck;
  report.sequence = liveScroll_ ? liveScroll_->write(report) : 0;
  lastLiveReport_ = report;
  publishedGesture_ = userScrolled || scrollPhase != SCROLL_PHASE_IDLE;
  return report;
}

ScrollPatch ScrollSync::push(const LiveScroll::Report& report) {
  lastPushedReport_ = report;
  hasPushedReport_ = true;
  ScrollPatch patch;
  patch.report = report;
  if (commandSequence_ > 0) {
    patch.hasCommand = true;
    patch.commandIndex = commandIndex_;
    patch.commandSequence = commandSequence_;
    patch.commandViewPosition = commandViewPosition_;
  }
  return patch;
}

ScrollPatch ScrollSync::livePatch(double offsetX, double offsetY, bool userScrolled, double scrollPhase) {
  return push(writeReport(offsetX, offsetY, userScrolled, scrollPhase, echoedToken_, mounted_.concealGeneration));
}

ScrollPatch ScrollSync::livePatch(double offsetX, double offsetY) {
  return livePatch(offsetX, offsetY, currentUserScrolled(), currentScrollPhase());
}

ScrollPatch ScrollSync::reportPatch(const LiveScroll::Report& report) {
  return push(writeReport(report.offsetX, report.offsetY, report.userScrolled, report.scrollPhase,
    static_cast<std::uint64_t>(report.commitToken), report.concealGenerationAck));
}

std::optional<ScrollPatch> ScrollSync::clearUserScrolled(double offsetX, double offsetY) {
  /*
   * Skip only when neither the mounted state nor our last report was a gesture. The mounted
   * state alone is not enough: after a pull past the top the bounce back sends one report that
   * may not have mounted yet, and later corrections would be dropped as if a gesture took over.
   */
  if (!publishedGesture_ && !mounted_.userScrolled && mounted_.scrollPhase == SCROLL_PHASE_IDLE) {
    return std::nullopt;
  }
  return livePatch(offsetX, offsetY, false, SCROLL_PHASE_IDLE);
}

ScrollPatch ScrollSync::issueCommand(
  double index, double viewPosition, double offsetX, double offsetY, bool momentumYielded) {
  commandIndex_ = index;
  commandViewPosition_ = viewPosition;
  commandSequence_ = std::max(mounted_.commandSequence, commandSequence_) + 1;
  ScrollPatch patch = momentumYielded ? livePatch(offsetX, offsetY, false, SCROLL_PHASE_IDLE)
                                      : livePatch(offsetX, offsetY);
  patch.offsetEnabled = true;
  return patch;
}

bool ScrollSync::currentUserScrolled() const {
  return lastLiveReport_.sequence > 0 || !hasMounted_ ? lastLiveReport_.userScrolled : mounted_.userScrolled;
}

double ScrollSync::currentScrollPhase() const {
  return lastLiveReport_.sequence > 0 || !hasMounted_ ? lastLiveReport_.scrollPhase : mounted_.scrollPhase;
}

}
