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
   * the view, and counts as applied.
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

  /*
   * The core estimated where the newest animated command lands. Animate there unless a finger
   * or a drag owns the view, which gives the command up.
   */
  if (commandAnimated_ && state.animationSequence == commandSequence_ && state.animationSequence > animatedSequence_) {
    animatedSequence_ = state.animationSequence;
    if (!view.touching && !view.ownsOffset) {
      landing_ = true;
      action.kind = MountAction::Kind::Animate;
      action.offsetX = horizontal_ ? state.animationOffset : view.offsetX;
      action.offsetY = horizontal_ ? view.offsetY : state.animationOffset;
      return action;
    }
  }

  /*
   * An animated command owns the view until it lands. A write now would cut the animation
   * short, and the landing command replaces the correction anyway.
   */
  if (landing_ && armedAnimated_) {
    return action;
  }

  // While a row is dragged or dropping the drag owns the offset.
  if (!state.offsetEnabled || view.ownsOffset) {
    return action;
  }

  double offsetX = state.offsetX;
  double offsetY = state.offsetY;

  /*
   * A moving view has gone on by the time a correction mounts. Add the correction to the
   * live offset instead. Corrections made during a gesture or already shifted keep shifting
   * after the motion stops.
   */
  bool continuesShiftedCorrection = token != 0 && token == shiftedToken_;
  bool computedDuringGesture = token != 0 && (state.userScrolled || state.scrollPhase != SCROLL_PHASE_IDLE);
  bool shift = view.moving || continuesShiftedCorrection || computedDuringGesture;
  if (shift) {
    double delta = target - base;
    // The core resends the full correction until it is echoed. Shift only by what is left.
    if (token != 0) {
      double applied = 0.0;
      if (token == shiftedToken_) {
        applied = shiftedTokenDelta_;
      } else if (token == writtenToken_) {
        applied = writtenTokenDelta_;
      }
      double unapplied = delta - applied;
      shiftedToken_ = token;
      shiftedTokenDelta_ = delta;
      delta = unapplied;
    }
    double shifted = std::min(std::max(along(view.shiftFromX, view.shiftFromY) + delta, view.minOffset),
      std::max(view.minOffset, view.maxOffset));
    offsetX = horizontal_ ? shifted : view.offsetX;
    offsetY = horizontal_ ? view.offsetY : shifted;
  } else if (token != 0) {
    /*
     * The write applies the whole correction. A later mount of the same token that shifts,
     * like Android mounting the state again once the write moved the view, adds only what is left.
     */
    writtenToken_ = token;
    writtenTokenDelta_ = target - base;
  }

  /*
   * Corrections with a token and shifted ones keep a fling going. A write with no token may
   * carry an offset a frame old, and our own animated scrolls use a plain write too.
   */
  action.preserveMomentum = (token != 0 || shift) && !isArmedAnimated();

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
    release();
  }
}

bool ScrollSync::concealAckDue(bool jumpPending) const {
  return mounted_.concealGeneration != 0.0 && mounted_.offsetEnabled && !reportedDuringMount_ && !jumpPending;
}

FrameReport ScrollSync::onScroll(const ScrollFrame& frame) {
  FrameReport result;
  double offsetX = frame.offsetX;
  double offsetY = frame.offsetY;

  /*
   * A frame after our own write is ours wherever it landed and echoes the token. An animated
   * scroll of ours is ours on every frame until it reaches its target.
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
        result.landed = true;
        armed_ = false;
        armedAnimated_ = false;
        armedExact_ = false;
        armedToken_ = 0;
      }
    } else {
      echoedToken_ = armedToken_;
      if (landed && armedExact_) {
        offsetX = armedX_;
        offsetY = armedY_;
      }
      release();
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

void ScrollSync::release() {
  armed_ = false;
  armedAnimated_ = false;
  armedExact_ = false;
  armedToken_ = 0;
}

void ScrollSync::disarm() {
  release();
  // A finger or a new command takes over from an animated command, started or not.
  landing_ = false;
  if (commandAnimated_) {
    animatedSequence_ = std::max(animatedSequence_, commandSequence_);
  }
}

void ScrollSync::momentumStopped() {
  disarm();
  publishedGesture_ = false;
}

/*
 * Whether a scroll frame must become a state update. Anything but the offset moving inside the
 * band sends it.
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
  if (report.userScrolled != pushedReport_.userScrolled || report.scrollPhase != pushedReport_.scrollPhase ||
      report.commitToken != pushedReport_.commitToken ||
      report.concealGenerationAck != pushedReport_.concealGenerationAck) {
    return true;
  }
  // The view clamped the offset to a new content size, or a mount moved it.
  if (applyingContentSize_ || mounting_ || commitEveryFrame) {
    return true;
  }
  // The offset left the band the mounted layout pass published. An empty band never holds it.
  return !mounted_.band.contains(along(report.offsetX, report.offsetY));
}

LiveScroll::Report ScrollSync::writeReport(
  double offsetX,
  double offsetY,
  bool userScrolled,
  double scrollPhase,
  std::uint64_t token,
  double concealGenerationAck) {
  LiveScroll::Report report;
  report.offsetX = offsetX;
  report.offsetY = offsetY;
  report.userScrolled = userScrolled;
  report.scrollPhase = scrollPhase;
  report.commitToken = token;
  report.concealGenerationAck = concealGenerationAck;
  report.sequence = liveScroll_ ? liveScroll_->write(report) : 0;
  liveReport_ = report;
  publishedGesture_ = userScrolled || scrollPhase != SCROLL_PHASE_IDLE;
  return report;
}

ScrollPatch ScrollSync::push(const LiveScroll::Report& report) {
  pushedReport_ = report;
  hasPushedReport_ = true;
  ScrollPatch patch;
  patch.report = report;
  if (commandSequence_ > 0) {
    patch.hasCommand = true;
    patch.commandIndex = commandIndex_;
    patch.commandSequence = commandSequence_;
    patch.commandViewPosition = commandViewPosition_;
    patch.commandRowOffset = commandRowOffset_;
    patch.commandAnimated = commandAnimated_;
  }
  if (anchorRequestSequence_ > 0) {
    patch.hasAnchorRequest = true;
    patch.anchorRequestSequence = anchorRequestSequence_;
  }
  return patch;
}

ScrollPatch ScrollSync::livePatch(double offsetX, double offsetY, bool userScrolled, double scrollPhase) {
  return push(writeReport(offsetX, offsetY, userScrolled, scrollPhase, echoedToken_, mounted_.concealGeneration));
}

ScrollPatch ScrollSync::livePatch(double offsetX, double offsetY) {
  return livePatch(offsetX, offsetY, isCurrentUserScrolled(), getCurrentScrollPhase());
}

ScrollPatch ScrollSync::reportPatch(const LiveScroll::Report& report) {
  return push(writeReport(report.offsetX, report.offsetY, report.userScrolled, report.scrollPhase,
    report.commitToken, report.concealGenerationAck));
}

std::optional<ScrollPatch> ScrollSync::clearUserScrolled(double offsetX, double offsetY) {
  /*
   * Skip only when neither the mounted state nor our newest report was a gesture, since the
   * mounted state can lag behind a bounce back report.
   */
  if (!publishedGesture_ && !mounted_.userScrolled && mounted_.scrollPhase == SCROLL_PHASE_IDLE) {
    return std::nullopt;
  }
  return livePatch(offsetX, offsetY, false, SCROLL_PHASE_IDLE);
}

ScrollPatch ScrollSync::issueCommand(
  const ScrollCommand& command,
  double offsetX,
  double offsetY,
  bool momentumYielded) {
  // A command replaces an animated one still on its way.
  landing_ = false;
  if (commandAnimated_) {
    animatedSequence_ = std::max(animatedSequence_, commandSequence_);
  }
  commandIndex_ = command.index;
  commandViewPosition_ = command.viewPosition;
  commandRowOffset_ = command.rowOffset;
  commandAnimated_ = command.animated;
  commandSequence_ = std::max(mounted_.commandSequence, commandSequence_) + 1;
  ScrollPatch patch = momentumYielded ? livePatch(offsetX, offsetY, false, SCROLL_PHASE_IDLE)
                                      : livePatch(offsetX, offsetY);
  // An animated command moves nothing yet. The core only estimates where it lands.
  patch.offsetEnabled = !command.animated;
  return patch;
}

std::optional<ScrollPatch> ScrollSync::land(double offsetX, double offsetY) {
  if (!landing_) {
    return std::nullopt;
  }
  // The animation was ours to its last frame. Its end is not the user.
  if (armedAnimated_) {
    echoedToken_ = armedToken_;
    armed_ = false;
    armedAnimated_ = false;
    armedExact_ = false;
    armedToken_ = 0;
  }
  ScrollCommand command{commandIndex_, commandViewPosition_, commandRowOffset_, false};
  return issueCommand(command, offsetX, offsetY, true);
}

ScrollPatch ScrollSync::requestAnchor(double offsetX, double offsetY) {
  anchorRequestSequence_ += 1;
  return livePatch(offsetX, offsetY);
}

bool ScrollSync::isCurrentUserScrolled() const {
  return liveReport_.sequence > 0 || !hasMounted_ ? liveReport_.userScrolled : mounted_.userScrolled;
}

double ScrollSync::getCurrentScrollPhase() const {
  return liveReport_.sequence > 0 || !hasMounted_ ? liveReport_.scrollPhase : mounted_.scrollPhase;
}

}
