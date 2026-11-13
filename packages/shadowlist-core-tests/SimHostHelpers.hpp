#pragma once

#include "FuzzHelpers.hpp"
#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include "VirtualizationHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/KeyList.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/ListCommit.hpp>
#include <shadowlist-core/host/ListLayout.hpp>
#include <shadowlist-core/host/MountedRange.hpp>
#include <shadowlist-core/host/ScrollSync.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/*
 * SimHost, a small model of the Fabric host around one Container, and what the core fuzz
 * tests do to it and check after: data changes that hold the reader, user scrolls, scroll
 * commands and the checks every frame has to pass.
 */
namespace slt {

using namespace azimgd::shadowlist;

/*
 * In SimHost::deferredPasses, a row that is not late or whose late measurement came in.
 */
inline constexpr int LATE_ROW_LANDED = -1;

/*
 * The coverage kind an operation of the core is counted under, and its name in a trace.
 */
inline const char* operationKind(OperationType type) {
  switch (type) {
    case OperationType::MaintainAnchor:
      return "core maintain anchor";
    case OperationType::ScrollToKey:
      return "core scroll to key";
    case OperationType::ScrollToStart:
      return "core scroll to start";
    case OperationType::ScrollToEnd:
      return "core scroll to end";
    case OperationType::BottomPin:
      return "core bottom pin";
    case OperationType::ShrinkClamp:
      return "core shrink clamp";
  }
  return "core unknown operation";
}

/*
 * Ways a real host strays from the ideal one, which writes every correction at once and
 * reports it back on the next frame. Each fault is switched on its own and all are off unless
 * a test turns them on. The host counts a fault into the episode under its kind every time
 * it happens. HOST_FAULTS has the kinds.
 */
struct HostFaults {
  /*
   * The write of a correction waits for some mounts, like while a row drag owns the offset
   * or a newer Fabric revision replaces the one that carried it.
   */
  bool lateEchoes = false;

  /*
   * A correction is never written while SimHost::droppingEchoes is set, which a test does
   * for a few frames. The core can only keep asking.
   */
  bool droppedEchoes = false;

  /*
   * The core runs twice on the input of one frame, like a Fabric commit that lost a race
   * and ran again.
   */
  bool repeatedEchoes = false;

  /*
   * A report taken right before a write arrives after it, with the offset and the token
   * from before.
   */
  bool staleEchoes = false;

  /*
   * A finger moves the view between a correction that holds a row and its write.
   */
  bool scrollsBeforeEchoes = false;

  /*
   * A row that enters the mounted range off screen is measured some layout passes later,
   * like a row React has not rendered yet. Off screen is outside both the view and the
   * viewport the core is sending it to.
   */
  bool lateMeasurements = false;

  /*
   * The same for a row on screen, which shows as a blank cell until then.
   */
  bool lateMeasurementsOnScreen = false;

  /*
   * The rows a data change deleted stay mounted for one more layout pass, and the host has
   * sizes for them.
   */
  bool deletedKeyMeasurements = false;
};

/*
 * A fault and the kind the host counts it under.
 */
struct HostFault {
  bool HostFaults::* flag;
  const char* kind;
};

inline constexpr HostFault HOST_FAULTS[] = {
  {&HostFaults::lateEchoes, "late echo"},
  {&HostFaults::droppedEchoes, "dropped echo"},
  {&HostFaults::repeatedEchoes, "repeated echo"},
  {&HostFaults::staleEchoes, "stale echo"},
  {&HostFaults::scrollsBeforeEchoes, "scroll before echo"},
  {&HostFaults::lateMeasurements, "late measurement"},
  {&HostFaults::lateMeasurementsOnScreen, "late measurement on screen"},
  {&HostFaults::deletedKeyMeasurements, "deleted key measurement"},
};

inline const char* faultKind(bool HostFaults::* flag) {
  for (const HostFault& fault : HOST_FAULTS) {
    if (fault.flag == flag) {
      return fault.kind;
    }
  }
  fail("a fault without a kind in HOST_FAULTS");
}

/*
 * The kinds a host with these faults counts into its episodes.
 */
inline std::vector<std::string> faultKinds(const HostFaults& faults) {
  std::vector<std::string> kinds;
  for (const HostFault& fault : HOST_FAULTS) {
    if (faults.*fault.flag) {
      kinds.push_back(fault.kind);
    }
  }
  return kinds;
}

/*
 * A stand in for the scroll view and the Fabric layout pass around one Container.
 */
struct SimHost : FuzzHost {
  Container container;
  std::vector<std::string> keys;

  double header = 0.0;
  double footer = 0.0;
  double windowSize = WINDOW_HEIGHT;
  bool inverted = false;
  bool followAppends = false;
  std::size_t mountPad = DEFAULT_OVERSCAN_ROWS;

  /*
   * What the scroll view holds and reports.
   */
  double hostOffset = 0.0;
  std::uint64_t echoedToken = 0;
  std::uint64_t echoedRevision = 0;
  ScrollPhase phase = ScrollPhase::Idle;
  bool userScrolled = false;

  /*
   * The last state the host mounted, base included, and the view's own scroll protocol.
   */
  ScrollSync sync;
  double stateOffset = 0.0;
  double stateBase = 0.0;
  std::uint64_t stateToken = 0;
  std::uint64_t stateRevision = 0;

  /*
   * Whether the last layout pass changed a row's size. The core picked its measured range
   * before those rows moved.
   */
  bool layoutResized = false;

  /*
   * The token of the previous operation counted into the episode.
   */
  std::uint64_t countedToken = 0;

  /*
   * Called after each update of commit and open, before the layout pass. That is the frame
   * the core's edge callbacks judge.
   */
  std::function<void()> afterUpdate;

  /*
   * How the host misbehaves. The faults draw from faultRng. The generator's picks from rng
   * then stay the same when a fault is switched off.
   */
  const HostFaults faults;
  std::mt19937 faultRng;

  /*
   * What the echo faults keep between frames: whether corrections are dropped right now, the
   * token a fault already acted on, the mounts a late write still waits, the report kept
   * from before a write, and the finger that moved the view before one. travel is how far
   * such fingers moved the view since a test last cleared it.
   */
  bool droppingEchoes = false;
  bool landingCommand = false;
  std::uint64_t faultedToken = 0;
  int withheldMounts = 0;
  bool stalePending = false;
  double staleOffset = 0.0;
  std::uint64_t staleToken = 0;
  std::uint64_t staleRevision = 0;
  ScrollPhase stalePhase = ScrollPhase::Idle;
  bool staleUserScrolled = false;
  bool fingerDown = false;
  bool fingerMoved = false;
  double travel = 0.0;

  /*
   * What the measurement faults keep: the layout passes each late row still waits, whether
   * the last pass skipped a late row or took one in, the keys the last pass had mounted, and
   * the first thing a deleted row's size did that it must not.
   */
  std::unordered_map<std::string, int> deferredPasses;
  bool measuresPending = false;
  std::vector<std::string> mountedKeys;
  std::string finding;

  SimHost(std::uint32_t seed, FuzzEpisode& episode, const HostFaults& faults = {})
    : FuzzHost(seed, episode), faults(faults), faultRng(~seed) {}

  double maxOffset() const {
    return std::max(0.0, axisTotal() - windowSize);
  }

  /*
   * The content size and the core's offset along the scroll axis.
   */
  double axisTotal() const {
    return container.horizontal ? container.revision.contentWidth : container.revision.contentHeight;
  }

  double axisOffset() const {
    return container.horizontal ? container.revision.offsetX : container.revision.offsetY;
  }

  /*
   * Count a fault into the episode. Returns its kind.
   */
  std::string reachFault(bool HostFaults::* flag) {
    const char* kind = faultKind(flag);
    episode.reach(kind);
    return kind;
  }

  /*
   * Roll for a second run of the core on this frame's input and count it. Returns whether
   * the core runs again.
   */
  bool rollRepeatedEcho() {
    if (!faults.repeatedEchoes || !chance(faultRng, 30)) {
      return false;
    }
    note(reachFault(&HostFaults::repeatedEchoes));
    return true;
  }

  /*
   * Decide whether this mount's correction stays unwritten, and count the fault that keeps
   * it. Only a correction with a token is withheld: the core publishes that one again until a
   * report confirms it. Returns whether the write is withheld.
   */
  bool withholdWrite(const ContainerStateUpdate& update) {
    if (!update.applyOffset || update.commitToken == 0) {
      return false;
    }
    bool firstMount = update.commitToken != faultedToken;
    if (droppingEchoes) {
      if (firstMount) {
        faultedToken = update.commitToken;
        note(reachFault(&HostFaults::droppedEchoes) + " tok=" + std::to_string(update.commitToken));
      }
      return true;
    }
    if (faults.lateEchoes && firstMount) {
      faultedToken = update.commitToken;
      if (chance(faultRng, 50)) {
        withheldMounts = pickInt(faultRng, 1, 4);
        std::string kind = reachFault(&HostFaults::lateEchoes);
        note(kind + " tok=" + std::to_string(update.commitToken) + " waits " + std::to_string(withheldMounts));
      }
    }
    if (withheldMounts > 0) {
      --withheldMounts;
      return true;
    }
    return false;
  }

  /*
   * A finger lands and moves the view right before the write of a correction that holds a
   * row. Scroll commands and end pins are left alone: a finger cancels those, and the core
   * then promises nothing about where the view rests. A landing helper excludes the finger
   * through the size corrections that settle its command. The finger only lands on a view the
   * core has the offset of. A move the host made itself, like a clamp to shorter content,
   * is reported first on a real host.
   */
  void nudgeBeforeWrite(const ContainerStateUpdate& update) {
    if (!faults.scrollsBeforeEchoes || landingCommand || !update.applyOffset || update.commitToken == 0 || fingerDown ||
        phase != ScrollPhase::Idle || userScrolled ||
        std::fabs(hostOffset - container.state.previousReportedOffset) >= OFFSET_MOVED_THRESHOLD) {
      return;
    }
    const std::optional<Operation>& running = container.state.operation;
    if (!running || running->type != OperationType::MaintainAnchor || !chance(faultRng, 40)) {
      return;
    }
    double moveBy = static_cast<double>(pickInt(faultRng, 1, 60)) * (chance(faultRng, 50) ? 1.0 : -1.0);
    // The view has not mounted the new range yet. Stay before the new end unless it is past it already.
    double landed = std::min(std::max(hostOffset + moveBy, 0.0), std::max(hostOffset, maxOffset()));
    fingerDown = true;
    fingerMoved = std::fabs(landed - hostOffset) >= 0.01;
    travel += landed - hostOffset;
    hostOffset = landed;
    note(reachFault(&HostFaults::scrollsBeforeEchoes) + " to off=" + std::to_string(hostOffset));
  }

  /*
   * Keep the report the view would send right before a write moves it. With a finger on the
   * view it is the finger's own frame.
   */
  void keepStaleReport() {
    if (!faults.staleEchoes || !chance(faultRng, 50)) {
      return;
    }
    stalePending = true;
    staleOffset = hostOffset;
    staleToken = echoedToken;
    staleRevision = echoedRevision;
    stalePhase = fingerDown ? ScrollPhase::Dragging : phase;
    staleUserScrolled = fingerDown ? fingerMoved : userScrolled;
  }

  /*
   * Whether a stretch of the content overlaps the viewport, either where the view is now or
   * where the core is sending it.
   */
  bool isOnScreen(double start, double extent, double coreOffset) const {
    auto overlaps = [&](double offset) { return start + extent > offset && start < offset + windowSize; };
    return overlaps(hostOffset) || overlaps(coreOffset);
  }

  /*
   * Decide whether the layout pass skips a row because it is not rendered yet, and count a
   * row that starts to wait. A row waits at most three passes, counted from the first pass
   * that wanted it. onScreen is isOnScreen for the row. The pass a late row lands in changes
   * sizes after the core picked its measured range, and the host is not at rest until the frame
   * after it. Returns whether the row is skipped.
   */
  bool deferMeasurement(const Row& row, bool onScreen) {
    if (row.measured || !(onScreen ? faults.lateMeasurementsOnScreen : faults.lateMeasurements)) {
      return false;
    }
    auto found = deferredPasses.find(row.key);
    if (found == deferredPasses.end()) {
      int passes = chance(faultRng, 50) ? pickInt(faultRng, 1, 3) : LATE_ROW_LANDED;
      found = deferredPasses.emplace(row.key, passes).first;
      if (passes > 0) {
        reachFault(onScreen ? &HostFaults::lateMeasurementsOnScreen : &HostFaults::lateMeasurements);
        note("late measurement " + row.key + " waits " + std::to_string(passes));
      }
    }
    if (found->second == LATE_ROW_LANDED) {
      return false;
    }
    measuresPending = true;
    if (found->second == 0) {
      found->second = LATE_ROW_LANDED;
      return false;
    }
    --found->second;
    return true;
  }

  /*
   * The children the previous pass mounted are still there. The Fabric host looks each one
   * up by key before it gives the core a size. A child whose row the data change deleted
   * must find no row, and a size predicted for it waits for a row instead of landing on one.
   */
  void measureDeletedRows() {
    const std::vector<Row>& rows = container.revision.rows;
    for (const std::string& key : mountedKeys) {
      std::size_t index = container.indexOfKey(key);
      if (index != UNDEFINED_INDEX) {
        if (finding.empty() && (index >= rows.size() || rows[index].key != key)) {
          finding = "key " + key + " resolves to row " + std::to_string(index) + ", which is not its own";
        }
        continue;
      }
      if (std::find(keys.begin(), keys.end(), key) != keys.end()) {
        if (finding.empty()) {
          finding = "row " + key + " is in the data but the core has no row for it";
        }
        continue;
      }
      reachFault(&HostFaults::deletedKeyMeasurements);
      std::size_t reflowed = Virtualizer::applyPredictedRowSize(container, key, {WINDOW_WIDTH, trueSizes[key]});
      if (finding.empty() && reflowed != UNDEFINED_INDEX) {
        finding = "a size for the deleted row " + key + " landed on row " + std::to_string(reflowed);
      }
    }
    mountedKeys.clear();
  }

  FrameInput input(bool ownWrite) {
    // The frame points at the host's keys, as Fabric's does at its props, and copies none.
    FrameInput frame = inputFor({}, ownWrite ? container.revision.offsetY : hostOffset);
    frame.keysRef = KeyList(keys);
    frame.windowHeight = windowSize;
    frame.headerSize = header;
    frame.footerSize = footer;
    frame.inverted = inverted;
    frame.followAppends = followAppends;
    frame.offsetEnabled = ownWrite;
    std::uint64_t running = container.state.operation ? container.state.operation->commitToken : 0;
    frame.commitToken = ownWrite ? running : echoedToken;
    frame.commitRevision = ownWrite ? stateRevision : echoedRevision;
    frame.scrollPhase = phase;
    frame.userScrolled = userScrolled;
    return frame;
  }

  /*
   * Measure the rows the host has mounted, the measured range and mountPad rows around it, in
   * one batch along the scroll axis. Rows that wait for a late measurement are left alone.
   */
  void measureMountedRows() {
    Container& core = container;
    layoutResized = false;
    measuresPending = false;
    IndexRange measured = core.getMeasuredRange();
    if (measured.low == UNDEFINED_INDEX || core.revision.rows.empty()) {
      Virtualizer::recomputeContentSize(core);
      return;
    }
    std::size_t low = measured.low - std::min(measured.low, mountPad);
    std::size_t high = std::min(core.revision.rows.size() - 1, measured.high + mountPad);
    double coreOffset = axisOffset();
    bool oneColumn = !core.horizontal && core.numberOfColumns <= 1;
    layoutResized = layoutRows(core, low, high, [&](std::size_t index) -> std::optional<Size> {
      const Row& row = core.revision.rows[index];
      if (faults.deletedKeyMeasurements) {
        mountedKeys.push_back(row.key);
      }
      if (deferMeasurement(row, isOnScreen(offsetOf(core, index), sizeOf(core, index), coreOffset))) {
        return std::nullopt;
      }
      double size = trueSizes[row.key];
      if (oneColumn) {
        return Size{WINDOW_WIDTH, size};
      }
      return core.horizontal ? Size{size, row.height} : Size{row.width, size};
    });
  }

  /*
   * The layout pass: header and window into the core, every mounted row measured, one reflow.
   */
  void layoutPass() {
    applyLayoutInputs(container, header, footer, WINDOW_WIDTH, windowSize);
    if (faults.deletedKeyMeasurements) {
      measureDeletedRows();
    }
    measureMountedRows();
  }

  /*
   * Mount the state the layout pass produced, as the scroll view would, through the hosts'
   * own ScrollSync: an idle view takes the offset, a moving one is shifted by the part of the
   * correction not applied yet.
   */
  void mountState() {
    Container& core = container;
    ContainerStateUpdate update = core.resolveStateUpdate(stateOffset, stateOffset,
      core.revision.contentWidth, core.revision.contentHeight);
    ListScrollState state;
    state.offsetY = stateOffset;
    state.baseY = stateBase;
    state.commitToken = stateToken;
    state.commitRevision = stateRevision;
    if (publishStateUpdate(state, update)) {
      stateOffset = state.offsetY;
      stateBase = state.baseY;
      stateToken = state.commitToken;
      stateRevision = state.commitRevision;
    }
    bool withheld = withholdWrite(update);
    if (!withheld) {
      nudgeBeforeWrite(update);
    }
    // The content size write clamps a resting view, but not one held past the edge by a finger.
    if (phase == ScrollPhase::Idle && !fingerDown && hostOffset > maxOffset()) {
      hostOffset = maxOffset();
    }

    MountedScroll mounted;
    mounted.offsetEnabled = update.applyOffset;
    mounted.offsetY = update.offsetY;
    mounted.baseY = stateBase;
    mounted.commitToken = update.commitToken;
    mounted.commitRevision = update.commitRevision;
    mounted.userScrolled = userScrolled;
    mounted.scrollPhase = phase == ScrollPhase::Dragging ? SCROLL_PHASE_DRAGGING
      : phase == ScrollPhase::Settling ? SCROLL_PHASE_SETTLING : SCROLL_PHASE_IDLE;
    ViewMotion view;
    view.offsetY = hostOffset;
    view.shiftFromY = hostOffset;
    view.maxOffset = maxOffset();
    view.moving = phase != ScrollPhase::Idle || userScrolled || fingerDown;
    // ScrollSync writes nothing while a row drag owns the offset.
    view.ownsOffset = withheld;
    sync.beginMount(mounted, nullptr);
    MountAction action = sync.correction(view);
    if (action.kind == MountAction::Kind::Write) {
      // The scroll view keeps a written offset inside its range.
      double applied = std::min(std::max(action.offsetY, 0.0), maxOffset());
      bool moved = std::fabs(applied - hostOffset) >= 0.01;
      sync.willWrite(action);
      if (moved) {
        keepStaleReport();
        hostOffset = applied;
        ScrollFrame frame;
        frame.offsetY = applied;
        sync.onScroll(frame);
        echoedToken = sync.getEchoedToken();
        echoedRevision = sync.getEchoedRevision();
        // The frame of our own write is not the finger's.
        fingerMoved = false;
      }
      sync.didWrite(moved);
      echoedToken = sync.getEchoedToken();
      echoedRevision = sync.getEchoedRevision();
    }
    sync.endMount();
  }

  /*
   * Fold what the host can see after a commit into the episode's digest, and count each
   * operation the core starts under its type.
   */
  void observeCommit() {
    const Container& core = container;
    auto measured = core.getMeasuredRange();
    episode.observe(hostOffset);
    episode.observe(stateOffset);
    episode.observe(echoedToken);
    episode.observe(core.revision.offsetX);
    episode.observe(core.revision.offsetY);
    episode.observe(core.revision.contentWidth);
    episode.observe(core.revision.contentHeight);
    episode.observe(core.revision.rows.size());
    episode.observe(measured.low);
    episode.observe(measured.high);
    episode.observe(core.state.anchor.key);
    episode.observe(core.state.anchor.offset);
    if (!core.state.operation) {
      episode.observe(0);
      return;
    }
    const Operation& operation = *core.state.operation;
    episode.observe(operation.commitToken);
    episode.observe(operation.type);
    if (operation.commitToken != countedToken) {
      countedToken = operation.commitToken;
      episode.reach(operationKind(operation.type));
    }
  }

  /*
   * One commit: update, layout, mount. Returns whether the core wrote an offset. stale is
   * set for the commit of a report kept from before a write, which runs ahead of the live one.
   */
  bool commit(bool ownWrite, bool stale = false) {
    /*
     * A stale report may make the core write again, and the report kept before that write
     * is older than the live one too. Reports arrive in the order the view sent them.
     */
    while (!ownWrite && !stale && stalePending) {
      stalePending = false;
      std::string kind = reachFault(&HostFaults::staleEchoes);
      note(kind + " off=" + std::to_string(staleOffset) + " tok=" + std::to_string(staleToken) +
        " rev=" + std::to_string(staleRevision));
      std::swap(phase, stalePhase);
      std::swap(userScrolled, staleUserScrolled);
      commit(false, true);
      std::swap(phase, stalePhase);
      std::swap(userScrolled, staleUserScrolled);
    }
    // The finger that moved the view before a write is still down for the next live report.
    bool fingerReport = !ownWrite && !stale && fingerDown;
    if (fingerReport) {
      phase = ScrollPhase::Dragging;
      userScrolled = fingerMoved;
    }
    // A host report patches the state with the live offset before Fabric commits it.
    if (!ownWrite) {
      stateOffset = hostOffset;
    }
    FrameInput frame = input(ownWrite);
    if (stale) {
      frame.offsetY = staleOffset;
      frame.commitToken = staleToken;
      frame.commitRevision = staleRevision;
      stateOffset = staleOffset;
    }
    /*
     * A report puts its token into the state too, and a correction the report does not echo
     * then starts from the reported offset.
     */
    if (!ownWrite) {
      stateToken = frame.commitToken;
      stateRevision = frame.commitRevision;
    }
    Virtualizer::update(container, frame);
    if (afterUpdate) {
      afterUpdate();
    }
    if (rollRepeatedEcho()) {
      Virtualizer::update(container, frame);
      if (afterUpdate) {
        afterUpdate();
      }
    }
    layoutPass();
    bool corrected = container.state.offsetCorrected;
    mountState();
    observeCommit();
    if (tracing) {
      const Container& core = container;
      IndexRange measured = core.getMeasuredRange();
      std::printf(
        "      %s off=%.1f core=%.1f total=%.1f max=%.1f n=%zu op=%s:%llu pend=%d rest=%d rel=%d init=%d tok=%llu rev=%llu "
        "win=[%zd..%zd] anchor=%s sub=%.1f\n",
        ownWrite ? "own " : "host", hostOffset, core.revision.offsetY, core.revision.contentHeight, maxOffset(),
        core.revision.rows.size(), core.state.operation ? operationKind(core.state.operation->type) : "none",
        static_cast<unsigned long long>(core.state.operation ? core.state.operation->commitToken : 0),
        core.state.pendingScrollToEnd ? 1 : 0, core.state.restingAtInvertedBottom ? 1 : 0,
        core.state.invertedBottomReleased ? 1 : 0, core.state.invertedInitialized ? 1 : 0,
        static_cast<unsigned long long>(echoedToken), static_cast<unsigned long long>(echoedRevision),
        static_cast<std::ptrdiff_t>(measured.low),
        static_cast<std::ptrdiff_t>(measured.high), core.state.anchor.key.c_str(), core.state.anchor.offset);
    }
    // The own write commit is what Fabric runs when state changes, without a host report.
    if (ownWrite || stale) {
      return corrected;
    }
    // A host report clears the one shot gesture flag once the core has seen it.
    userScrolled = false;
    if (fingerReport) {
      phase = ScrollPhase::Idle;
      fingerDown = false;
    }
    return corrected;
  }

  /*
   * Let every correction land: our own write, the host echo, until nothing moves.
   * Returns the number of frames it took, or -1 when it never settled.
   */
  int settle() {
    for (int frame = 0; frame < FUZZ_MAX_SETTLE_FRAMES; ++frame) {
      bool corrected = commit(true);
      bool reported = commit(false);
      if (!corrected && !reported && !container.state.operation && !container.state.pendingScrollToEnd &&
          !measuresPending) {
        return frame;
      }
    }
    return -1;
  }
};

/*
 * Layout invariants that must hold after every frame: every row follows the one before it on
 * its track, and a list of one column ends with its footer. Returns what is wrong, or nothing.
 */
inline std::string checkGeometry(const SimHost& host) {
  const Container& core = host.container;
  if (core.state.rowStructureDirty || core.state.rowSizeDirtyFromIndex != UNDEFINED_INDEX) {
    return "";
  }
  std::string gap = describeGeometryGap(core);
  if (!gap.empty() || core.horizontal || core.numberOfColumns > 1) {
    return gap;
  }
  double end = core.headerSize;
  if (!core.revision.rows.empty()) {
    std::size_t last = core.revision.rows.size() - 1;
    end = std::max(end, offsetOf(core, last) + sizeOf(core, last));
  }
  double total = end + core.footerSize;
  if (std::fabs(core.revision.contentHeight - total) > 0.01) {
    return "total " + std::to_string(core.revision.contentHeight) + " expected " + std::to_string(total);
  }
  return "";
}

/*
 * Every row that overlaps the viewport at offset must be mounted, or it is blank: inside the
 * core's measured range and the pad rows a host keeps around it. Returns what is wrong, or nothing.
 */
inline std::string checkMounted(const SimHost& host, double offset, std::size_t pad) {
  const Container& core = host.container;
  if (core.revision.rows.empty()) {
    return "";
  }
  IndexRange measured = core.getMeasuredRange();
  if (measured.low == UNDEFINED_INDEX) {
    return "no measured range";
  }
  std::size_t blank = firstRowNotMounted(core, offset, pad);
  if (blank == UNDEFINED_INDEX) {
    return "";
  }
  return "row " + std::to_string(blank) + " on screen but not mounted, measured [" + std::to_string(measured.low) +
    ".." + std::to_string(measured.high) + "] pad " + std::to_string(pad) + " offset=" + std::to_string(offset);
}

/*
 * The content rows on screen and where the top of each sits, judged from the host's offset.
 */
inline std::vector<ScreenRow> visibleRows(const SimHost& host) {
  std::vector<ScreenRow> rows;
  const Container& core = host.container;
  for (std::size_t index = firstRowEndingAfter(core, host.hostOffset); index < core.revision.rows.size(); ++index) {
    const Row& row = core.revision.rows[index];
    if (row.offsetY >= host.hostOffset + host.windowSize) {
      break;
    }
    if (core.isAnchorable(row.key)) {
      rows.push_back({row.key, row.offsetY - host.hostOffset});
    }
  }
  return rows;
}

inline ScreenRow firstVisibleRow(const SimHost& host) {
  std::vector<ScreenRow> rows = visibleRows(host);
  return rows.empty() ? ScreenRow{} : rows.front();
}

/*
 * Where a row's leading edge sits from the viewport start, or NaN for a key the list lost.
 */
inline double screenOf(const SimHost& host, const std::string& key) {
  std::size_t index = host.container.indexOfKey(key);
  if (index == UNDEFINED_INDEX) {
    return std::nan("");
  }
  return offsetOf(host.container, index) - host.hostOffset;
}

/*
 * Whether a hold of the row at screen was clamped to an edge of the scroll range.
 */
inline bool isHoldClamped(const SimHost& host, const std::string& key, double screen) {
  double wanted = offsetOf(host.container, host.container.indexOfKey(key)) - screen;
  return isHoldClamped(wanted, host.hostOffset, host.maxOffset());
}

/*
 * Where a scroll to the row of key rests, see landingOffset.
 */
inline double landingOf(const SimHost& host, const std::string& key, double viewPosition) {
  std::size_t index = host.container.indexOfKey(key);
  return landingOffset(offsetOf(host.container, index), sizeOf(host.container, index), viewPosition,
    host.windowSize, host.maxOffset());
}

/*
 * Geometry must hold after every frame. The measured range is only judged at rest, since a layout
 * pass that shrinks rows above the viewport moves rows into it before the next frame picks
 * the measured range again, and the mounted overscan covers that frame.
 */
inline void checkFrame(const SimHost& host, bool atRest = false) {
  if (!host.finding.empty()) {
    host.failWith("deleted rows: " + host.finding);
  }
  std::string geometry = checkGeometry(host);
  if (!geometry.empty()) {
    host.failWith("geometry: " + geometry);
  }
  if (!atRest) {
    return;
  }
  std::string coverage = checkMounted(host, host.axisOffset(), 0);
  if (!coverage.empty()) {
    host.failWith("coverage: " + coverage);
  }
}

/*
 * Bring the list to rest: no correction, every visible row measured, host at a real offset.
 */
inline void rest(SimHost& host) {
  host.phase = ScrollPhase::Idle;
  host.userScrolled = false;
  if (host.settle() < 0) {
    host.failWith("correction never settled at rest");
  }
  checkFrame(host, true);
}

/*
 * Apply a data change while the host is idle and check that the first row on screen stays put
 * once the correction lands. Rows removed by the change fall through to the next visible one.
 * expectBottom asks for the list to end at the bottom instead, like followAppends.
 * A finger that moved the view on the way, see HostFaults, takes the row along by its travel.
 */
inline void changeAndHold(SimHost& host, const std::vector<std::string>& nextKeys, const std::string& what) {
  std::vector<ScreenRow> before = visibleRows(host);
  double maxBefore = host.maxOffset();
  /*
   * At the bottom, and not released: a reader who scrolled up and was clamped back onto the
   * bottom by shrinking content is still counted as away, see the release tests.
   */
  bool wasAtBottom = host.inverted && host.hostOffset >= maxBefore - INVERTED_FOLLOW_BAND &&
    !host.container.state.invertedBottomReleased;
  bool appendOnly = nextKeys.size() > host.keys.size() &&
    std::equal(host.keys.begin(), host.keys.end(), nextKeys.begin());
  bool followsToBottom = wasAtBottom && host.followAppends && appendOnly;

  host.travel = 0.0;
  host.keys = nextKeys;
  host.note(what + " n=" + std::to_string(nextKeys.size()) + " off=" + std::to_string(host.hostOffset));
  host.commit(false);
  checkFrame(host);
  int frames = host.settle();
  if (frames < 0) {
    host.failWith(what + ": correction never settled");
  }
  checkFrame(host, true);

  if (followsToBottom) {
    if (std::fabs(host.hostOffset - host.maxOffset()) > FUZZ_TOLERANCE) {
      host.failWith(what + ": followAppends did not reach the bottom, off=" +
        std::to_string(host.hostOffset) + " max=" + std::to_string(host.maxOffset()));
    }
    return;
  }

  // The first row that survived the change is the one that should hold.
  for (const ScreenRow& row : before) {
    double after = screenOf(host, row.key);
    if (std::isnan(after)) {
      continue;
    }
    double expected = row.screen - host.travel;
    if (std::fabs(after - expected) <= FUZZ_TOLERANCE) {
      return;
    }
    /*
     * A hold that would put the offset outside the scroll range is clamped instead. With a
     * finger's travel the hold is clamped first, the travel is added, and the view clamps that
     * again. It rests the travel away from the edge at most.
     */
    std::size_t index = host.container.indexOfKey(row.key);
    double wanted = host.container.revision.rows[index].offsetY - expected;
    double held = host.container.revision.rows[index].offsetY - row.screen;
    double edgeSlack = FUZZ_TOLERANCE + std::fabs(host.travel);
    if (isHoldClamped(held, host.hostOffset, host.maxOffset(), edgeSlack) ||
        isHoldClamped(wanted, host.hostOffset, host.maxOffset(), edgeSlack)) {
      return;
    }
    /*
     * An inverted list resting on its newest row holds the bottom edge instead of the row.
     * The row may move when the list ends at the bottom.
     */
    if (host.inverted && wasAtBottom && std::fabs(host.hostOffset - host.maxOffset()) <= FUZZ_TOLERANCE) {
      return;
    }
    host.failWith(what + ": row " + row.key + " moved from " + std::to_string(row.screen) +
      " to " + std::to_string(after) + " (wanted offset " + std::to_string(wanted) + ", host " +
      std::to_string(host.hostOffset) + ", max " + std::to_string(host.maxOffset()) + ", frames " +
      std::to_string(frames) + ")");
  }
}

/*
 * A user scroll to a resting offset, reported as a drag and then an idle settle.
 */
inline void scrollTo(SimHost& host, double offset) {
  offset = std::min(std::max(offset, 0.0), host.maxOffset());
  host.note("scroll to " + std::to_string(offset));
  double travel = offset - host.hostOffset;
  int steps = 3;
  for (int step = 1; step <= steps; ++step) {
    host.hostOffset += travel / steps;
    host.phase = ScrollPhase::Dragging;
    host.userScrolled = true;
    host.commit(false);
    checkFrame(host);
  }
  host.hostOffset = offset;
  host.phase = ScrollPhase::Settling;
  host.userScrolled = true;
  host.commit(false);
  checkFrame(host);
  host.phase = ScrollPhase::Idle;
  host.userScrolled = false;
  host.commit(false);
  checkFrame(host);
  rest(host);
}

/*
 * Open a fresh list the way the app does: first commit before layout, then layout with the window.
 */
inline void open(SimHost& host, std::size_t count) {
  host.keys = host.makeKeys(count);
  host.note("open n=" + std::to_string(count) + (host.inverted ? " inverted" : ""));
  FrameInput first = host.input(false);
  first.windowWidth = 0.0;
  first.windowHeight = 0.0;
  Virtualizer::update(host.container, first);
  if (host.afterUpdate) {
    host.afterUpdate();
  }
  host.layoutPass();
  host.mountState();
  rest(host);
}

/*
 * The scrollToEnd command settles and leaves the view on the end.
 */
inline void scrollToEndAndLand(SimHost& host) {
  host.landingCommand = true;
  host.note("scrollToEnd");
  host.container.scrollToEnd();
  host.commit(false);
  if (host.settle() < 0) {
    host.failWith("scrollToEnd never settled");
  }
  checkFrame(host, true);
  if (std::fabs(host.hostOffset - host.maxOffset()) > FUZZ_TOLERANCE) {
    host.failWith("scrollToEnd landed at " + std::to_string(host.hostOffset) + " max " +
      std::to_string(host.maxOffset()));
  }
  host.landingCommand = false;
}

/*
 * The scrollToStart command settles and leaves the view on the start.
 */
inline void scrollToStartAndLand(SimHost& host) {
  host.landingCommand = true;
  host.note("scrollToStart");
  host.container.scrollToStart();
  host.commit(false);
  if (host.settle() < 0) {
    host.failWith("scrollToStart never settled");
  }
  checkFrame(host, true);
  if (host.hostOffset > FUZZ_TOLERANCE) {
    host.failWith("scrollToStart landed at " + std::to_string(host.hostOffset));
  }
  host.landingCommand = false;
}

inline std::vector<std::string> withPrepend(SimHost& host, std::size_t count) {
  std::vector<std::string> next = host.makeKeys(count);
  next.insert(next.end(), host.keys.begin(), host.keys.end());
  return next;
}

inline std::vector<std::string> withAppend(SimHost& host, std::size_t count) {
  std::vector<std::string> next = host.keys;
  std::vector<std::string> made = host.makeKeys(count);
  next.insert(next.end(), made.begin(), made.end());
  return next;
}

inline std::vector<std::string> withInsert(SimHost& host, std::size_t at, std::size_t count) {
  std::vector<std::string> next = host.keys;
  std::vector<std::string> made = host.makeKeys(count);
  at = std::min(at, next.size());
  next.insert(next.begin() + static_cast<std::ptrdiff_t>(at), made.begin(), made.end());
  return next;
}

inline std::vector<std::string> withRemove(SimHost& host, std::size_t at, std::size_t count) {
  std::vector<std::string> next = host.keys;
  if (next.empty()) {
    return next;
  }
  at = std::min(at, next.size() - 1);
  count = std::min(count, next.size() - at);
  next.erase(next.begin() + static_cast<std::ptrdiff_t>(at), next.begin() + static_cast<std::ptrdiff_t>(at + count));
  return next;
}


/*
 * The first row whose top is on screen that the reader also saw on screen last frame, at
 * previousOffset, with its real size. Only such a row can be seen to jump.
 * The core holds one row: the row at the top of the viewport, or the first row with a real
 * size that starts on screen when the top row has none. A row with a guessed size between that
 * row and the one the reader saw moves the rows after it when it gets measured, and no offset
 * holds both sides of it. Then nothing is judged.
 */
inline ScreenRow firstRowStartingOnScreen(const SimHost& host, double previousOffset) {
  bool pastCoreAnchor = false;
  const Container& core = host.container;
  for (std::size_t index = firstRowEndingAfter(core, host.hostOffset); index < core.revision.rows.size(); ++index) {
    const Row& row = core.revision.rows[index];
    double screen = row.offsetY - host.hostOffset;
    if (screen >= host.windowSize) {
      break;
    }
    if (!row.measured) {
      if (pastCoreAnchor) {
        return {};
      }
      continue;
    }
    pastCoreAnchor = true;
    bool sawIt = row.offsetY + row.height > previousOffset && row.offsetY < previousOffset + host.windowSize;
    if (screen < 0.0 || !sawIt) {
      continue;
    }
    return {row.key, screen};
  }
  return {};
}


}
