/*
 * Randomized host simulation. A small model of the Fabric host drives the core through
 * random data changes, gestures, header and window changes and scroll commands, and checks
 * after every frame that the layout is consistent, the window covers the viewport, every
 * correction settles, and the row the reader was looking at stays where it was.
 * Each test runs many seeds. A failure prints the seed and the steps that led to it. It
 * can be turned into a plain test.
 */

#include "TestFramework.hpp"
#include "TestHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/ListCommit.hpp>
#include <shadowlist-core/host/ScrollSync.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace azimgd::shadowlist;
using namespace slt;

namespace {

constexpr double TOLERANCE = 1.0;
constexpr int MAX_SETTLE_FRAMES = 24;

/*
 * Set SLT_FUZZ_TRACE=<seed> to print every frame of that seed.
 */
std::uint32_t traceSeed = [] {
  const char* value = std::getenv("SLT_FUZZ_TRACE");
  return value != nullptr ? static_cast<std::uint32_t>(std::strtoul(value, nullptr, 10)) : 0u;
}();

/*
 * A stand in for the scroll view and the Fabric layout pass around one Container.
 */
struct SimHost {
  Container container;
  std::vector<std::string> keys;
  std::unordered_map<std::string, double> trueSizes;
  std::mt19937 rng;

  double header = 0.0;
  double footer = 0.0;
  double windowSize = WINDOW_HEIGHT;
  bool inverted = false;
  bool followAppends = false;
  std::size_t mountPad = 4;
  std::size_t nextKey = 0;

  /*
   * What the scroll view holds and reports.
   */
  double hostOffset = 0.0;
  std::uint64_t echoedToken = 0;
  ScrollPhase phase = ScrollPhase::Idle;
  bool userScrolled = false;

  /*
   * The last state the host mounted, base included, and the view's own scroll protocol.
   */
  ScrollSync sync;
  double stateOffset = 0.0;
  double stateBase = 0.0;
  std::uint64_t stateToken = 0;

  std::vector<std::string> log;
  bool tracing = false;

  explicit SimHost(std::uint32_t seed) : rng(seed), tracing(seed == traceSeed) {}

  void note(const std::string& line) {
    if (tracing) {
      std::printf("      %s\n", line.c_str());
    }
    log.push_back(line);
    if (log.size() > 60) {
      log.erase(log.begin());
    }
  }

  std::string dump() const {
    std::ostringstream out;
    for (const std::string& line : log) {
      out << "\n    " << line;
    }
    return out.str();
  }

  double randomSize() {
    std::uniform_int_distribution<int> pick(0, 9);
    int bucket = pick(rng);
    if (bucket < 6) {
      return 40.0 + static_cast<double>(pick(rng)) * 12.0;
    }
    if (bucket < 9) {
      return 120.0 + static_cast<double>(pick(rng)) * 30.0;
    }
    return 400.0 + static_cast<double>(pick(rng)) * 60.0;
  }

  std::string makeKey() {
    std::string key = "r" + std::to_string(nextKey++);
    trueSizes[key] = randomSize();
    return key;
  }

  std::vector<std::string> makeKeys(std::size_t count) {
    std::vector<std::string> made;
    for (std::size_t index = 0; index < count; ++index) {
      made.push_back(makeKey());
    }
    return made;
  }

  double maxOffset() const {
    double total = container.revision.contentHeight;
    return std::max(0.0, total - windowSize);
  }

  FrameInput input(bool ownWrite) {
    FrameInput frame = inputFor(keys, ownWrite ? container.revision.offsetY : hostOffset);
    frame.windowHeight = windowSize;
    frame.headerSize = header;
    frame.footerSize = footer;
    frame.inverted = inverted;
    frame.followAppends = followAppends;
    frame.offsetEnabled = ownWrite;
    frame.commitToken = ownWrite ? (container.operation ? container.operation->commitToken : 0) : echoedToken;
    frame.scrollPhase = phase;
    frame.userScrolled = userScrolled;
    return frame;
  }

  /*
   * The layout pass: header and window into the core, every mounted row measured, one reflow.
   */
  void layoutPass() {
    Container& core = container;
    bool inputsChanged = core.headerSize != header || core.footerSize != footer ||
      core.revision.windowHeight != windowSize || core.revision.windowWidth != WINDOW_WIDTH;
    if (inputsChanged) {
      double previousHeader = core.headerSize;
      double previousWindow = core.getWindowSize();
      bool rowsMove = previousHeader != header || core.revision.windowWidth != WINDOW_WIDTH;
      core.headerSize = header;
      core.footerSize = footer;
      core.revision.windowWidth = WINDOW_WIDTH;
      core.revision.windowHeight = windowSize;
      if (rowsMove) {
        Virtualizer::recomputeRowOffsets(core, 0);
      }
      Virtualizer::applyHeaderSizeChange(core, previousHeader);
      Virtualizer::applyWindowSizeChange(core, previousWindow);
      core.offsetCorrected = true;
    }

    auto visible = core.getMeasuredRange();
    if (visible.low != UNDEFINED_INDEX && !core.revision.rows.empty()) {
      std::size_t low = visible.low;
      std::size_t high = visible.high;
      low = low > mountPad ? low - mountPad : 0;
      high = std::min(core.revision.rows.size() - 1, high + mountPad);
      std::size_t lowest = UNDEFINED_INDEX;
      for (std::size_t index = low; index <= high; ++index) {
        double size = trueSizes[core.revision.rows[index].key];
        if (Virtualizer::applyRowSize(core, index, {WINDOW_WIDTH, size}) && index < lowest) {
          lowest = index;
        }
      }
      if (lowest != UNDEFINED_INDEX) {
        Virtualizer::commitRowSizes(core, lowest);
      }
    }
    Virtualizer::recomputeContentSize(core);
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
    if (publishStateUpdate(state, update)) {
      stateOffset = state.offsetY;
      stateBase = state.baseY;
      stateToken = state.commitToken;
    }
    // The content size write clamps a resting view, but not one held past the edge by a finger.
    if (phase == ScrollPhase::Idle && hostOffset > maxOffset()) {
      hostOffset = maxOffset();
    }

    MountedScroll mounted;
    mounted.offsetEnabled = update.applyOffset;
    mounted.offsetY = update.offsetY;
    mounted.baseY = stateBase;
    mounted.commitToken = update.commitToken;
    mounted.userScrolled = userScrolled;
    mounted.scrollPhase = phase == ScrollPhase::Dragging ? SCROLL_PHASE_DRAGGING
      : phase == ScrollPhase::Settling ? SCROLL_PHASE_SETTLING : SCROLL_PHASE_IDLE;
    ViewMotion view;
    view.offsetY = hostOffset;
    view.shiftFromY = hostOffset;
    view.maxOffset = maxOffset();
    view.moving = phase != ScrollPhase::Idle || userScrolled;
    sync.beginMount(mounted, nullptr);
    MountAction action = sync.correction(view);
    if (action.kind == MountAction::Kind::Write) {
      // The scroll view keeps a written offset inside its range.
      double applied = std::min(std::max(action.offsetY, 0.0), maxOffset());
      bool moved = std::fabs(applied - hostOffset) >= 0.01;
      sync.willWrite(action);
      if (moved) {
        hostOffset = applied;
        ScrollFrame frame;
        frame.offsetY = applied;
        sync.onScroll(frame);
        echoedToken = sync.getEchoedToken();
      }
      sync.didWrite(moved);
    }
    sync.endMount();
  }

  /*
   * One commit: update, layout, mount. Returns whether the core wrote an offset.
   */
  bool commit(bool ownWrite) {
    // A host report patches the state with the live offset before Fabric commits it.
    if (!ownWrite) {
      stateOffset = hostOffset;
    }
    Virtualizer::update(container, input(ownWrite));
    layoutPass();
    bool corrected = container.offsetCorrected;
    mountState();
    if (traceSeed != 0 && tracing) {
      const Container& core = container;
      std::printf("      %s off=%.1f core=%.1f total=%.1f max=%.1f n=%zu op=%s%llu pend=%d rest=%d rel=%d init=%d tok=%llu win=[%zd..%zd] anchor=%s sub=%.1f\n",
        ownWrite ? "own " : "host", hostOffset, core.revision.offsetY, core.revision.contentHeight,
        maxOffset(), core.revision.rows.size(),
        core.operation ? (core.operation->type == OperationType::MaintainAnchor ? "hold:" :
          core.operation->type == OperationType::ScrollToKey ? "key:" : core.operation->type == OperationType::ScrollToEnd ? "end:" :
          core.operation->type == OperationType::BottomPin ? "pin:" : core.operation->type == OperationType::ShrinkClamp ? "clamp:" : "start:") : "none",
        static_cast<unsigned long long>(core.operation ? core.operation->commitToken : 0),
        core.pendingScrollToEnd ? 1 : 0, core.restingAtInvertedBottom ? 1 : 0, core.invertedBottomReleased ? 1 : 0,
        core.invertedInitialized ? 1 : 0, static_cast<unsigned long long>(echoedToken),
        static_cast<std::ptrdiff_t>(core.getMeasuredRange().low), static_cast<std::ptrdiff_t>(core.getMeasuredRange().high),
        core.anchor.key.c_str(), core.anchor.offset);
    }
    // The own write commit is what Fabric runs when state changes, without a host report.
    if (ownWrite) {
      return corrected;
    }
    // A host report clears the one shot gesture flag once the core has seen it.
    userScrolled = false;
    return corrected;
  }

  /*
   * Let every correction land: our own write, the host echo, until nothing moves.
   * Returns the number of frames it took, or -1 when it never settled.
   */
  int settle() {
    for (int frame = 0; frame < MAX_SETTLE_FRAMES; ++frame) {
      bool corrected = commit(true);
      bool reported = commit(false);
      if (!corrected && !reported && !container.operation && !container.pendingScrollToEnd) {
        return frame;
      }
    }
    return -1;
  }
};

/*
 * Layout invariants that must hold after every frame.
 */
std::string checkGeometry(const SimHost& host) {
  const Container& core = host.container;
  const std::vector<Row>& rows = core.revision.rows;
  if (core.rowStructureDirty || core.rowSizeDirtyFromIndex != UNDEFINED_INDEX) {
    return "";
  }
  double expected = core.headerSize;
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const Row& row = rows[index];
    if (!std::isfinite(row.offsetY) || !std::isfinite(row.height)) {
      return "non finite geometry at " + std::to_string(index);
    }
    if (std::fabs(row.offsetY - expected) > 0.01) {
      return "row " + std::to_string(index) + " at " + std::to_string(row.offsetY) + " expected " + std::to_string(expected);
    }
    if (row.index != index) {
      return "row " + std::to_string(index) + " carries index " + std::to_string(row.index);
    }
    expected += row.height;
  }
  double total = std::max(expected, core.headerSize) + core.footerSize;
  if (std::fabs(core.revision.contentHeight - total) > 0.01) {
    return "total " + std::to_string(core.revision.contentHeight) + " expected " + std::to_string(total);
  }
  return "";
}

/*
 * Every row that overlaps the viewport must be inside the measured window, or it is blank.
 */
std::string checkCoverage(const SimHost& host) {
  const Container& core = host.container;
  if (core.revision.rows.empty()) {
    return "";
  }
  auto visible = core.getMeasuredRange();
  std::size_t low = visible.low;
  std::size_t high = visible.high;
  double offset = core.revision.offsetY;
  double end = offset + core.getWindowSize();
  for (std::size_t index = 0; index < core.revision.rows.size(); ++index) {
    const Row& row = core.revision.rows[index];
    if (row.offsetY + row.height <= offset || row.offsetY >= end) {
      continue;
    }
    if (visible.low == UNDEFINED_INDEX) {
      return "no window while row " + std::to_string(index) + " overlaps the viewport";
    }
    if (index < low || index > high) {
      return "row " + std::to_string(index) + " overlaps the viewport outside window [" +
        std::to_string(low) + ".." + std::to_string(high) + "] offset=" + std::to_string(offset);
    }
  }
  return "";
}

/*
 * The first content row on screen and where its top sits, judged from the host's offset.
 */
struct ScreenRow {
  std::string key;
  double screenY = 0.0;
};

ScreenRow firstVisibleRow(const SimHost& host) {
  const Container& core = host.container;
  for (const Row& row : core.revision.rows) {
    if (row.offsetY + row.height <= host.hostOffset) {
      continue;
    }
    if (row.offsetY >= host.hostOffset + host.windowSize) {
      break;
    }
    if (core.isAnchorable(row.key)) {
      return {row.key, row.offsetY - host.hostOffset};
    }
  }
  return {};
}

std::vector<ScreenRow> visibleRows(const SimHost& host) {
  std::vector<ScreenRow> rows;
  const Container& core = host.container;
  for (const Row& row : core.revision.rows) {
    if (row.offsetY + row.height <= host.hostOffset) {
      continue;
    }
    if (row.offsetY >= host.hostOffset + host.windowSize) {
      break;
    }
    if (core.isAnchorable(row.key)) {
      rows.push_back({row.key, row.offsetY - host.hostOffset});
    }
  }
  return rows;
}

double screenYOf(const SimHost& host, const std::string& key) {
  std::size_t index = host.container.indexOfKey(key);
  if (index == UNDEFINED_INDEX) {
    return std::nan("");
  }
  return host.container.revision.rows[index].offsetY - host.hostOffset;
}

void failWith(const SimHost& host, std::uint32_t seed, const std::string& what) {
  fail("seed " + std::to_string(seed) + ": " + what + host.dump());
}

/*
 * Geometry must hold after every frame. The window is only judged at rest, since a layout
 * pass that shrinks rows above the viewport moves rows into it before the next frame picks
 * the window again, and the mounted overscan covers that frame.
 */
void checkFrame(const SimHost& host, std::uint32_t seed, bool atRest = false) {
  std::string geometry = checkGeometry(host);
  if (!geometry.empty()) {
    failWith(host, seed, "geometry: " + geometry);
  }
  if (!atRest) {
    return;
  }
  std::string coverage = checkCoverage(host);
  if (!coverage.empty()) {
    failWith(host, seed, "coverage: " + coverage);
  }
}

/*
 * Bring the list to rest: no correction, every visible row measured, host at a real offset.
 */
void rest(SimHost& host, std::uint32_t seed) {
  host.phase = ScrollPhase::Idle;
  host.userScrolled = false;
  if (host.settle() < 0) {
    failWith(host, seed, "correction never settled at rest");
  }
  checkFrame(host, seed, true);
}

/*
 * Apply a data change while the host is idle and check that the first row on screen stays put
 * once the correction lands. Rows removed by the change fall through to the next visible one.
 * expectBottom asks for the list to end at the bottom instead, like followAppends.
 */
void changeAndHold(
  SimHost& host,
  std::uint32_t seed,
  const std::vector<std::string>& nextKeys,
  const std::string& what) {
  std::vector<ScreenRow> before = visibleRows(host);
  double maxBefore = host.maxOffset();
  /*
   * At the bottom, and not released: a reader who scrolled up and was clamped back onto the
   * bottom by shrinking content is still counted as away, see the release tests.
   */
  bool wasAtBottom = host.inverted && host.hostOffset >= maxBefore - INVERTED_FOLLOW_BAND &&
    !host.container.invertedBottomReleased;
  bool appendOnly = nextKeys.size() > host.keys.size() &&
    std::equal(host.keys.begin(), host.keys.end(), nextKeys.begin());
  bool followsToBottom = wasAtBottom && host.followAppends && appendOnly;

  host.keys = nextKeys;
  host.note(what + " n=" + std::to_string(nextKeys.size()) + " off=" + std::to_string(host.hostOffset));
  host.commit(false);
  checkFrame(host, seed);
  int frames = host.settle();
  if (frames < 0) {
    failWith(host, seed, what + ": correction never settled");
  }
  checkFrame(host, seed, true);

  if (followsToBottom) {
    if (std::fabs(host.hostOffset - host.maxOffset()) > TOLERANCE) {
      failWith(host, seed, what + ": followAppends did not reach the bottom, off=" +
        std::to_string(host.hostOffset) + " max=" + std::to_string(host.maxOffset()));
    }
    return;
  }

  // The first row that survived the change is the one that should hold.
  for (const ScreenRow& row : before) {
    double after = screenYOf(host, row.key);
    if (std::isnan(after)) {
      continue;
    }
    if (std::fabs(after - row.screenY) <= TOLERANCE) {
      return;
    }
    /*
     * A hold that would put the offset outside the scroll range is clamped instead.
     * Then the view rests on the edge it hit.
     */
    std::size_t index = host.container.indexOfKey(row.key);
    double wanted = host.container.revision.rows[index].offsetY - row.screenY;
    if (wanted < 0.0 && host.hostOffset <= TOLERANCE) {
      return;
    }
    if (wanted > host.maxOffset() && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE) {
      return;
    }
    /*
     * An inverted list resting on its newest row holds the bottom edge instead of the row.
     * The row may move when the list ends at the bottom.
     */
    if (host.inverted && wasAtBottom && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE) {
      return;
    }
    failWith(host, seed, what + ": row " + row.key + " moved from " + std::to_string(row.screenY) +
      " to " + std::to_string(after) + " (wanted offset " + std::to_string(wanted) + ", host " +
      std::to_string(host.hostOffset) + ", max " + std::to_string(host.maxOffset()) + ", frames " +
      std::to_string(frames) + ")");
  }
}

/*
 * A user scroll to a resting offset, reported as a drag and then an idle settle.
 */
void scrollTo(SimHost& host, std::uint32_t seed, double offset) {
  offset = std::min(std::max(offset, 0.0), host.maxOffset());
  host.note("scroll to " + std::to_string(offset));
  double travel = offset - host.hostOffset;
  int steps = 3;
  for (int step = 1; step <= steps; ++step) {
    host.hostOffset += travel / steps;
    host.phase = ScrollPhase::Dragging;
    host.userScrolled = true;
    host.commit(false);
    checkFrame(host, seed);
  }
  host.hostOffset = offset;
  host.phase = ScrollPhase::Settling;
  host.userScrolled = true;
  host.commit(false);
  checkFrame(host, seed);
  host.phase = ScrollPhase::Idle;
  host.userScrolled = false;
  host.commit(false);
  checkFrame(host, seed);
  rest(host, seed);
}

/*
 * Open a fresh list the way the app does: first commit before layout, then layout with the window.
 */
void open(SimHost& host, std::uint32_t seed, std::size_t count) {
  host.keys = host.makeKeys(count);
  host.note("open n=" + std::to_string(count) + (host.inverted ? " inverted" : ""));
  FrameInput first = host.input(false);
  first.windowWidth = 0.0;
  first.windowHeight = 0.0;
  Virtualizer::update(host.container, first);
  host.layoutPass();
  host.mountState();
  rest(host, seed);
}

std::vector<std::string> withPrepend(SimHost& host, std::size_t count) {
  std::vector<std::string> next = host.makeKeys(count);
  next.insert(next.end(), host.keys.begin(), host.keys.end());
  return next;
}

std::vector<std::string> withAppend(SimHost& host, std::size_t count) {
  std::vector<std::string> next = host.keys;
  std::vector<std::string> made = host.makeKeys(count);
  next.insert(next.end(), made.begin(), made.end());
  return next;
}

std::vector<std::string> withInsert(SimHost& host, std::size_t at, std::size_t count) {
  std::vector<std::string> next = host.keys;
  std::vector<std::string> made = host.makeKeys(count);
  at = std::min(at, next.size());
  next.insert(next.begin() + static_cast<std::ptrdiff_t>(at), made.begin(), made.end());
  return next;
}

std::vector<std::string> withRemove(SimHost& host, std::size_t at, std::size_t count) {
  std::vector<std::string> next = host.keys;
  if (next.empty()) {
    return next;
  }
  at = std::min(at, next.size() - 1);
  count = std::min(count, next.size() - at);
  next.erase(next.begin() + static_cast<std::ptrdiff_t>(at), next.begin() + static_cast<std::ptrdiff_t>(at + count));
  return next;
}

}

/*
 * A plain vertical feed: prepends, appends, inserts and removes above, inside and below the
 * viewport, at random resting offsets. The first row on screen never moves.
 */
TEST(fuzz_idle_data_changes_hold_the_first_visible_row) {
  for (std::uint32_t seed = 1; seed <= 300; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.header = coin(host.rng) < 50 ? 0.0 : 120.0 + coin(host.rng);
    host.footer = coin(host.rng) < 70 ? 0.0 : 60.0;
    open(host, seed, 20 + static_cast<std::size_t>(coin(host.rng)));

    for (int step = 0; step < 12; ++step) {
      int roll = coin(host.rng);
      std::size_t count = 1 + static_cast<std::size_t>(coin(host.rng) % 12);
      std::size_t size = host.keys.size();
      auto visible = host.container.getMeasuredRange();
      std::size_t visLow = visible.low;
      std::size_t visHigh = visible.high;
      if (roll < 25) {
        changeAndHold(host, seed, withPrepend(host, count), "prepend " + std::to_string(count));
      } else if (roll < 40) {
        changeAndHold(host, seed, withAppend(host, count), "append " + std::to_string(count));
      } else if (roll < 55) {
        std::size_t at = static_cast<std::size_t>(coin(host.rng)) % (size + 1);
        changeAndHold(host, seed, withInsert(host, at, count), "insert " + std::to_string(count) + " at " + std::to_string(at));
      } else if (roll < 70) {
        std::size_t at = static_cast<std::size_t>(coin(host.rng)) % size;
        changeAndHold(host, seed, withRemove(host, at, count), "remove " + std::to_string(count) + " at " + std::to_string(at));
      } else if (roll < 80 && visLow != UNDEFINED_INDEX) {
        // Remove the row at the top of the screen along with a prepend, like a refresh.
        std::vector<std::string> next = withRemove(host, visLow, 1);
        std::vector<std::string> made = host.makeKeys(count);
        next.insert(next.begin(), made.begin(), made.end());
        changeAndHold(host, seed, next, "refresh dropping " + std::to_string(visLow));
      } else if (roll < 88 && visHigh != UNDEFINED_INDEX) {
        // Insert right inside the viewport, below the first row.
        std::size_t at = std::min(size, visLow + 1);
        changeAndHold(host, seed, withInsert(host, at, count), "insert in view at " + std::to_string(at));
      } else {
        double target = static_cast<double>(coin(host.rng)) / 99.0 * host.maxOffset();
        scrollTo(host, seed, target);
      }
      if (host.keys.empty()) {
        break;
      }
    }
  }
}

/*
 * An inverted chat: history pages prepended, incoming rows appended, with and without
 * followAppends, from the bottom and from up in the history.
 */
TEST(fuzz_inverted_history_and_incoming_hold_the_reader) {
  for (std::uint32_t seed = 1; seed <= 300; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.inverted = true;
    host.followAppends = coin(host.rng) < 50;
    host.header = coin(host.rng) < 60 ? 0.0 : 48.0;
    open(host, seed, 15 + static_cast<std::size_t>(coin(host.rng) % 40));
    if (std::fabs(host.hostOffset - host.maxOffset()) > TOLERANCE) {
      failWith(host, seed, "inverted list did not open at the bottom: off=" + std::to_string(host.hostOffset) +
        " max=" + std::to_string(host.maxOffset()));
    }

    for (int step = 0; step < 12; ++step) {
      int roll = coin(host.rng);
      std::size_t count = 1 + static_cast<std::size_t>(coin(host.rng) % 10);
      if (roll < 30) {
        changeAndHold(host, seed, withPrepend(host, count), "history " + std::to_string(count));
      } else if (roll < 60) {
        changeAndHold(host, seed, withAppend(host, count), "incoming " + std::to_string(count));
      } else if (roll < 70) {
        std::size_t at = static_cast<std::size_t>(coin(host.rng)) % host.keys.size();
        changeAndHold(host, seed, withRemove(host, at, 1), "delete " + std::to_string(at));
      } else if (roll < 85) {
        double target = static_cast<double>(coin(host.rng)) / 99.0 * host.maxOffset();
        scrollTo(host, seed, target);
      } else {
        scrollTo(host, seed, host.maxOffset());
      }
      if (host.keys.empty()) {
        break;
      }
    }
  }
}

/*
 * Header and footer resize while the reader rests. Off screen headers keep the rows still,
 * a visible header pushes them by its growth. The footer never moves a row.
 */
TEST(fuzz_header_and_footer_changes_keep_the_rows) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.header = 100.0;
    open(host, seed, 40);
    for (int step = 0; step < 10; ++step) {
      if (coin(host.rng) < 40) {
        double target = static_cast<double>(coin(host.rng)) / 99.0 * host.maxOffset();
        scrollTo(host, seed, target);
        continue;
      }
      ScreenRow row = firstVisibleRow(host);
      double offsetBefore = host.hostOffset;
      double previousHeader = host.header;
      bool footerChange = coin(host.rng) < 30;
      if (footerChange) {
        host.footer = host.footer > 0.0 ? 0.0 : 80.0;
        host.note("footer -> " + std::to_string(host.footer));
      } else {
        host.header = static_cast<double>(coin(host.rng)) * 2.0;
        host.note("header " + std::to_string(previousHeader) + " -> " + std::to_string(host.header) +
          " off=" + std::to_string(host.hostOffset));
      }
      // The header changes in a layout pass with no data change. Run just that and settle.
      host.commit(false);
      checkFrame(host, seed);
      if (host.settle() < 0) {
        failWith(host, seed, "header change never settled");
      }
      checkFrame(host, seed, true);
      if (row.key.empty()) {
        continue;
      }
      double after = screenYOf(host, row.key);
      double delta = host.header - previousHeader;
      bool headerOffScreen = offsetBefore > 0.0 && offsetBefore >= previousHeader;
      double expected = footerChange || headerOffScreen ? row.screenY : row.screenY + delta;
      std::size_t index = host.container.indexOfKey(row.key);
      double wanted = host.container.revision.rows[index].offsetY - expected;
      bool clamped = (wanted < 0.0 && host.hostOffset <= TOLERANCE) ||
        (wanted > host.maxOffset() && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE);
      if (std::fabs(after - expected) > TOLERANCE && !clamped) {
        failWith(host, seed, "row " + row.key + " at " + std::to_string(after) + " expected " + std::to_string(expected) +
          " (host " + std::to_string(host.hostOffset) + ")");
      }
    }
  }
}

/*
 * Scroll commands land where they aim and settle, with data changes in between.
 */
TEST(fuzz_scroll_commands_land_and_settle) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.inverted = coin(host.rng) < 30;
    host.header = coin(host.rng) < 50 ? 0.0 : 90.0;
    open(host, seed, 30 + static_cast<std::size_t>(coin(host.rng)));
    for (int step = 0; step < 8; ++step) {
      int roll = coin(host.rng);
      if (roll < 40) {
        std::size_t index = static_cast<std::size_t>(coin(host.rng)) % host.keys.size();
        double viewPosition = (coin(host.rng) % 3) * 0.5;
        std::string key = host.keys[index];
        host.note("scrollToRow " + std::to_string(index) + " pos " + std::to_string(viewPosition));
        host.container.scrollToRow(index, viewPosition);
        host.commit(false);
        if (host.settle() < 0) {
          failWith(host, seed, "scrollToRow never settled");
        }
        checkFrame(host, seed, true);
        std::size_t landed = host.container.indexOfKey(key);
        double rowSize = host.container.revision.rows[landed].height;
        double freeSpace = host.windowSize - rowSize;
        double expectedScreen = freeSpace > 0.0 ? viewPosition * freeSpace : 0.0;
        double wanted = host.container.revision.rows[landed].offsetY - expectedScreen;
        double clampedWanted = std::min(std::max(wanted, 0.0), host.maxOffset());
        if (std::fabs(host.hostOffset - clampedWanted) > TOLERANCE) {
          failWith(host, seed, "scrollToRow landed at " + std::to_string(host.hostOffset) + " wanted " +
            std::to_string(clampedWanted));
        }
      } else if (roll < 55) {
        host.note("scrollToEnd");
        host.container.scrollToEnd();
        host.commit(false);
        if (host.settle() < 0) {
          failWith(host, seed, "scrollToEnd never settled");
        }
        checkFrame(host, seed, true);
        if (std::fabs(host.hostOffset - host.maxOffset()) > TOLERANCE) {
          failWith(host, seed, "scrollToEnd landed at " + std::to_string(host.hostOffset) + " max " +
            std::to_string(host.maxOffset()));
        }
      } else if (roll < 70) {
        host.note("scrollToStart");
        host.container.scrollToStart();
        host.commit(false);
        if (host.settle() < 0) {
          failWith(host, seed, "scrollToStart never settled");
        }
        checkFrame(host, seed, true);
        if (host.hostOffset > TOLERANCE) {
          failWith(host, seed, "scrollToStart landed at " + std::to_string(host.hostOffset));
        }
      } else if (roll < 85) {
        std::size_t count = 1 + static_cast<std::size_t>(coin(host.rng) % 8);
        changeAndHold(host, seed, withPrepend(host, count), "prepend " + std::to_string(count));
      } else {
        std::size_t count = 1 + static_cast<std::size_t>(coin(host.rng) % 8);
        changeAndHold(host, seed, withAppend(host, count), "append " + std::to_string(count));
      }
    }
  }
}

/*
 * The whole dataset is swapped, shrunk or emptied. The list must never end up past its end,
 * never leave the viewport uncovered, and an emptied inverted list must pin again when rows return.
 */
TEST(fuzz_dataset_swaps_and_shrinks_stay_in_range) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.inverted = coin(host.rng) < 40;
    open(host, seed, 30 + static_cast<std::size_t>(coin(host.rng)));
    for (int step = 0; step < 8; ++step) {
      int roll = coin(host.rng);
      if (roll < 30) {
        double target = static_cast<double>(coin(host.rng)) / 99.0 * host.maxOffset();
        scrollTo(host, seed, target);
      } else if (roll < 55) {
        std::size_t keep = static_cast<std::size_t>(coin(host.rng) % 6);
        std::vector<std::string> next(host.keys.begin(), host.keys.begin() + static_cast<std::ptrdiff_t>(std::min(keep, host.keys.size())));
        host.keys = next;
        host.note("shrink to " + std::to_string(keep) + " off=" + std::to_string(host.hostOffset));
        host.commit(false);
        checkFrame(host, seed);
        if (host.settle() < 0) {
          failWith(host, seed, "shrink never settled");
        }
        checkFrame(host, seed, true);
        if (host.hostOffset > host.maxOffset() + TOLERANCE) {
          failWith(host, seed, "rests past the end after shrink: off=" + std::to_string(host.hostOffset) +
            " max=" + std::to_string(host.maxOffset()));
        }
      } else if (roll < 80) {
        std::vector<std::string> next = host.makeKeys(10 + static_cast<std::size_t>(coin(host.rng) % 40));
        host.keys = next;
        host.note("swap to " + std::to_string(next.size()) + " off=" + std::to_string(host.hostOffset));
        host.commit(false);
        checkFrame(host, seed);
        if (host.settle() < 0) {
          failWith(host, seed, "swap never settled");
        }
        checkFrame(host, seed, true);
        if (host.hostOffset > host.maxOffset() + TOLERANCE) {
          failWith(host, seed, "rests past the end after swap: off=" + std::to_string(host.hostOffset));
        }
        if (host.inverted && host.hostOffset < host.maxOffset() - TOLERANCE) {
          failWith(host, seed, "inverted swap did not rest at the bottom: off=" + std::to_string(host.hostOffset) +
            " max=" + std::to_string(host.maxOffset()));
        }
      } else {
        std::size_t count = 1 + static_cast<std::size_t>(coin(host.rng) % 8);
        if (host.keys.empty()) {
          host.keys = host.makeKeys(count);
          host.note("refill " + std::to_string(count));
          host.commit(false);
          if (host.settle() < 0) {
            failWith(host, seed, "refill never settled");
          }
          checkFrame(host, seed);
        } else {
          changeAndHold(host, seed, withAppend(host, count), "append " + std::to_string(count));
        }
      }
    }
  }
}

/*
 * Data lands while the finger is down or the list is settling. The row the reader held is
 * where it was minus how far they scrolled since.
 */
TEST(fuzz_changes_during_gestures_follow_the_finger) {
  for (std::uint32_t seed = 1; seed <= 300; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.inverted = coin(host.rng) < 30;
    host.header = coin(host.rng) < 50 ? 0.0 : 100.0;
    open(host, seed, 40 + static_cast<std::size_t>(coin(host.rng)));
    // Start somewhere in the middle so both directions have room.
    scrollTo(host, seed, host.maxOffset() * 0.5);

    for (int gesture = 0; gesture < 4; ++gesture) {
      // The rows on screen when the data lands, and the travel since then.
      std::vector<ScreenRow> held;
      double travel = 0.0;
      int frames = 3 + coin(host.rng) % 6;
      int changeAt = coin(host.rng) % frames;
      double perFrame = (static_cast<double>(coin(host.rng)) - 50.0) * 4.0;
      bool changed = false;
      std::string what;
      for (int frame = 0; frame < frames; ++frame) {
        double next = std::min(std::max(host.hostOffset + perFrame, 0.0), host.maxOffset());
        if (changed) {
          travel += next - host.hostOffset;
        }
        host.hostOffset = next;
        host.phase = frame < frames - 2 ? ScrollPhase::Dragging : ScrollPhase::Settling;
        host.userScrolled = true;
        if (frame == changeAt) {
          held = visibleRows(host);
          std::size_t count = 1 + static_cast<std::size_t>(coin(host.rng) % 8);
          int roll = coin(host.rng);
          if (roll < 50) {
            host.keys = withPrepend(host, count);
            what = "prepend " + std::to_string(count);
          } else if (roll < 75) {
            host.keys = withAppend(host, count);
            what = "append " + std::to_string(count);
          } else {
            std::size_t at = static_cast<std::size_t>(coin(host.rng)) % host.keys.size();
            host.keys = withInsert(host, at, count);
            what = "insert " + std::to_string(count) + " at " + std::to_string(at);
          }
          host.note(what + " mid gesture at off=" + std::to_string(host.hostOffset) + " travel=" + std::to_string(travel));
          changed = true;
        } else {
          host.note("gesture frame off=" + std::to_string(host.hostOffset));
        }
        host.commit(false);
        checkFrame(host, seed);
        // The state the pass produced mounts while the finger keeps moving.
        host.commit(true);
        checkFrame(host, seed);
      }
      host.phase = ScrollPhase::Idle;
      host.userScrolled = false;
      host.commit(false);
      checkFrame(host, seed);
      int settled = host.settle();
      if (settled < 0) {
        failWith(host, seed, what + ": correction never settled after the gesture");
      }
      checkFrame(host, seed, true);
      if (!changed) {
        continue;
      }
      for (const ScreenRow& row : held) {
        double after = screenYOf(host, row.key);
        if (std::isnan(after)) {
          continue;
        }
        double expected = row.screenY - travel;
        if (std::fabs(after - expected) <= TOLERANCE) {
          break;
        }
        std::size_t index = host.container.indexOfKey(row.key);
        double wanted = host.container.revision.rows[index].offsetY - expected;
        if (wanted < 0.0 && host.hostOffset <= TOLERANCE) {
          break;
        }
        if (wanted > host.maxOffset() && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE) {
          break;
        }
        failWith(host, seed, what + ": row " + row.key + " at " + std::to_string(after) + " expected " +
          std::to_string(expected) + " (travel " + std::to_string(travel) + ", host " + std::to_string(host.hostOffset) +
          ", max " + std::to_string(host.maxOffset()) + ")");
      }
    }
  }
}

namespace {

/*
 * The same host with a grid or a horizontal axis. Rows keep their scroll axis size in
 * trueSizes and the layout pass feeds it on the right axis.
 */
struct AxisHost : SimHost {
  std::size_t numberOfColumns = 1;
  bool horizontal = false;

  explicit AxisHost(std::uint32_t seed) : SimHost(seed) {}

  FrameInput axisInput(bool ownWrite) {
    FrameInput frame = input(ownWrite);
    frame.numberOfColumns = numberOfColumns;
    frame.horizontal = horizontal;
    if (horizontal) {
      frame.windowWidth = windowSize;
      frame.windowHeight = WINDOW_WIDTH;
      // Our own write comes back on the scroll axis, which is x here.
      frame.offsetX = ownWrite ? container.revision.offsetX : hostOffset;
      frame.offsetY = 0.0;
      frame.estimatedRowSize = {ESTIMATED_ROW_HEIGHT, WINDOW_WIDTH};
    }
    return frame;
  }
};

double axisOffset(const Container& core) {
  return core.horizontal ? core.revision.offsetX : core.revision.offsetY;
}

double axisTotal(const Container& core) {
  return core.horizontal ? core.revision.contentWidth : core.revision.contentHeight;
}

/*
 * One commit for the axis host: update, a layout pass that measures the mounted rows along
 * the scroll axis, then the idle mount of the state.
 */
bool axisCommit(AxisHost& host, bool ownWrite) {
  Container& core = host.container;
  if (!ownWrite) {
    host.stateOffset = host.hostOffset;
  }
  Virtualizer::update(core, host.axisInput(ownWrite));

  // Layout pass along the axis.
  auto visible = core.getMeasuredRange();
  if (visible.low != UNDEFINED_INDEX && !core.revision.rows.empty()) {
    std::size_t low = visible.low;
    std::size_t high = visible.high;
    low = low > host.mountPad ? low - host.mountPad : 0;
    high = std::min(core.revision.rows.size() - 1, high + host.mountPad);
    std::size_t lowest = UNDEFINED_INDEX;
    for (std::size_t index = low; index <= high; ++index) {
      const Row& row = core.revision.rows[index];
      double size = host.trueSizes[row.key];
      Size fed = host.horizontal ? Size{size, row.height} : Size{row.width, size};
      if (Virtualizer::applyRowSize(core, index, fed) && index < lowest) {
        lowest = index;
      }
    }
    if (lowest != UNDEFINED_INDEX) {
      Virtualizer::commitRowSizes(core, lowest);
    }
  }
  Virtualizer::recomputeContentSize(core);

  bool corrected = core.offsetCorrected;
  ContainerStateUpdate state = core.resolveStateUpdate(host.stateOffset, host.stateOffset,
    core.revision.contentWidth, core.revision.contentHeight);
  double maxOffset = std::max(0.0, axisTotal(core) - host.windowSize);
  if (host.hostOffset > maxOffset) {
    host.hostOffset = maxOffset;
  }
  if (state.changed) {
    host.stateOffset = host.horizontal ? state.offsetX : state.offsetY;
  }
  if (state.applyOffset) {
    double applied = std::min(std::max(host.horizontal ? state.offsetX : state.offsetY, 0.0), maxOffset);
    if (std::fabs(applied - host.hostOffset) >= 0.01) {
      host.hostOffset = applied;
      host.echoedToken = state.commitToken;
    }
  }
  host.userScrolled = false;
  if (host.tracing) {
    std::printf("      %s off=%.1f core=%.1f total=%.1f max=%.1f n=%zu op=%llu corrected=%d applied=%d anchor=%s sub=%.1f win=[%zd..%zd]\n",
      ownWrite ? "own " : "host", host.hostOffset, axisOffset(core), axisTotal(core), maxOffset, core.revision.rows.size(),
      static_cast<unsigned long long>(core.operation ? core.operation->commitToken : 0), corrected ? 1 : 0,
      state.applyOffset ? 1 : 0, core.anchor.key.c_str(), core.anchor.offset,
      static_cast<std::ptrdiff_t>(core.getMeasuredRange().low), static_cast<std::ptrdiff_t>(core.getMeasuredRange().high));
  }
  return corrected;
}

int axisSettle(AxisHost& host) {
  for (int frame = 0; frame < MAX_SETTLE_FRAMES; ++frame) {
    bool corrected = axisCommit(host, true);
    bool reported = axisCommit(host, false);
    if (!corrected && !reported && !host.container.operation && !host.container.pendingScrollToEnd) {
      return frame;
    }
  }
  return -1;
}

/*
 * Per track contiguity for a grid, along either axis.
 */
std::string checkAxisGeometry(const AxisHost& host) {
  const Container& core = host.container;
  if (core.rowStructureDirty || core.rowSizeDirtyFromIndex != UNDEFINED_INDEX) {
    return "";
  }
  std::size_t columns = std::max<std::size_t>(1, core.numberOfColumns);
  std::vector<double> edges(columns, core.headerSize);
  for (std::size_t index = 0; index < core.revision.rows.size(); ++index) {
    const Row& row = core.revision.rows[index];
    double start = core.horizontal ? row.offsetX : row.offsetY;
    double size = core.horizontal ? row.width : row.height;
    std::size_t track = index % columns;
    if (!std::isfinite(start) || !std::isfinite(size)) {
      return "non finite geometry at " + std::to_string(index);
    }
    if (std::fabs(start - edges[track]) > 0.01) {
      return "row " + std::to_string(index) + " at " + std::to_string(start) + " expected " + std::to_string(edges[track]);
    }
    edges[track] = start + size;
  }
  return "";
}

/*
 * First anchorable row overlapping the viewport, in index order. In a grid the tracks have
 * independent heights. One offset can only hold one track: the core holds the first
 * overlapping row by index, and so does this check.
 */
ScreenRow firstAxisRow(const AxisHost& host) {
  const Container& core = host.container;
  // A grid keeps holding the row it already holds while that row is on screen.
  if (host.numberOfColumns > 1 && !core.anchor.key.empty()) {
    std::size_t index = core.indexOfKey(core.anchor.key);
    if (index != UNDEFINED_INDEX) {
      const Row& row = core.revision.rows[index];
      double start = core.horizontal ? row.offsetX : row.offsetY;
      double size = core.horizontal ? row.width : row.height;
      if (start + size > host.hostOffset && start < host.hostOffset + host.windowSize) {
        return {row.key, start - host.hostOffset};
      }
    }
  }
  for (const Row& row : core.revision.rows) {
    double start = core.horizontal ? row.offsetX : row.offsetY;
    double size = core.horizontal ? row.width : row.height;
    if (start + size <= host.hostOffset || start >= host.hostOffset + host.windowSize) {
      continue;
    }
    if (!core.isAnchorable(row.key)) {
      continue;
    }
    return {row.key, start - host.hostOffset};
  }
  return {};
}

double axisScreenOf(const AxisHost& host, const std::string& key) {
  std::size_t index = host.container.indexOfKey(key);
  if (index == UNDEFINED_INDEX) {
    return std::nan("");
  }
  const Row& row = host.container.revision.rows[index];
  return (host.horizontal ? row.offsetX : row.offsetY) - host.hostOffset;
}

void axisRest(AxisHost& host, std::uint32_t seed) {
  if (axisSettle(host) < 0) {
    failWith(host, seed, "correction never settled at rest");
  }
  std::string geometry = checkAxisGeometry(host);
  if (!geometry.empty()) {
    failWith(host, seed, "geometry: " + geometry);
  }
}

void axisScrollTo(AxisHost& host, std::uint32_t seed, double offset) {
  double maxOffset = std::max(0.0, axisTotal(host.container) - host.windowSize);
  offset = std::min(std::max(offset, 0.0), maxOffset);
  host.note("scroll to " + std::to_string(offset));
  host.hostOffset = offset;
  host.phase = ScrollPhase::Dragging;
  host.userScrolled = true;
  axisCommit(host, false);
  host.phase = ScrollPhase::Idle;
  host.userScrolled = false;
  axisCommit(host, false);
  axisRest(host, seed);
}

/*
 * A data change on a grid or horizontal list holds the row at the top of the screen. Only
 * one track can be held in a grid. The check is on the anchor's own track.
 */
void axisChangeAndHold(
  AxisHost& host,
  std::uint32_t seed,
  const std::vector<std::string>& nextKeys,
  const std::string& what) {
  ScreenRow row = firstAxisRow(host);
  host.keys = nextKeys;
  host.note(what + " n=" + std::to_string(nextKeys.size()) + " off=" + std::to_string(host.hostOffset));
  axisCommit(host, false);
  if (axisSettle(host) < 0) {
    failWith(host, seed, what + ": correction never settled");
  }
  std::string geometry = checkAxisGeometry(host);
  if (!geometry.empty()) {
    failWith(host, seed, "geometry: " + geometry);
  }
  if (row.key.empty()) {
    return;
  }
  double after = axisScreenOf(host, row.key);
  if (std::isnan(after) || std::fabs(after - row.screenY) <= TOLERANCE) {
    return;
  }
  std::size_t index = host.container.indexOfKey(row.key);
  const Row& placed = host.container.revision.rows[index];
  double wanted = (host.horizontal ? placed.offsetX : placed.offsetY) - row.screenY;
  double maxOffset = std::max(0.0, axisTotal(host.container) - host.windowSize);
  if (wanted < 0.0 && host.hostOffset <= TOLERANCE) {
    return;
  }
  if (wanted > maxOffset && std::fabs(host.hostOffset - maxOffset) <= TOLERANCE) {
    return;
  }
  failWith(host, seed, what + ": row " + row.key + " moved from " + std::to_string(row.screenY) + " to " +
    std::to_string(after) + " (host " + std::to_string(host.hostOffset) + ", max " + std::to_string(maxOffset) + ")");
}

void axisOpen(AxisHost& host, std::uint32_t seed, std::size_t count) {
  host.keys = host.makeKeys(count);
  host.note("open n=" + std::to_string(count) + " cols=" + std::to_string(host.numberOfColumns) + (host.horizontal ? " horizontal" : ""));
  FrameInput first = host.axisInput(false);
  first.windowWidth = 0.0;
  first.windowHeight = 0.0;
  Virtualizer::update(host.container, first);
  axisCommit(host, false);
  axisRest(host, seed);
}

}

/*
 * Horizontal lists: the same holds along x.
 */
TEST(fuzz_horizontal_changes_hold_the_first_visible_column) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    AxisHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.horizontal = true;
    host.header = coin(host.rng) < 50 ? 0.0 : 80.0;
    axisOpen(host, seed, 20 + static_cast<std::size_t>(coin(host.rng)));
    for (int step = 0; step < 10; ++step) {
      int roll = coin(host.rng);
      std::size_t count = 1 + static_cast<std::size_t>(coin(host.rng) % 8);
      if (roll < 30) {
        axisChangeAndHold(host, seed, withPrepend(host, count), "prepend " + std::to_string(count));
      } else if (roll < 50) {
        axisChangeAndHold(host, seed, withAppend(host, count), "append " + std::to_string(count));
      } else if (roll < 65) {
        std::size_t at = static_cast<std::size_t>(coin(host.rng)) % (host.keys.size() + 1);
        axisChangeAndHold(host, seed, withInsert(host, at, count), "insert " + std::to_string(count) + " at " + std::to_string(at));
      } else if (roll < 80) {
        std::size_t at = static_cast<std::size_t>(coin(host.rng)) % host.keys.size();
        axisChangeAndHold(host, seed, withRemove(host, at, count), "remove " + std::to_string(count) + " at " + std::to_string(at));
      } else {
        double maxOffset = std::max(0.0, axisTotal(host.container) - host.windowSize);
        axisScrollTo(host, seed, static_cast<double>(coin(host.rng)) / 99.0 * maxOffset);
      }
      if (host.keys.empty()) {
        break;
      }
    }
  }
}

/*
 * Grids: changes by whole rows of tracks hold the top row. The geometry stays contiguous
 * per track for any change.
 */
TEST(fuzz_grid_changes_by_whole_rows_hold_the_top_row) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    AxisHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.numberOfColumns = 2 + static_cast<std::size_t>(coin(host.rng) % 3);
    host.header = coin(host.rng) < 50 ? 0.0 : 80.0;
    axisOpen(host, seed, 30 + static_cast<std::size_t>(coin(host.rng)));
    for (int step = 0; step < 10; ++step) {
      int roll = coin(host.rng);
      std::size_t count = host.numberOfColumns * (1 + static_cast<std::size_t>(coin(host.rng) % 4));
      if (roll < 35) {
        axisChangeAndHold(host, seed, withPrepend(host, count), "prepend " + std::to_string(count));
      } else if (roll < 55) {
        axisChangeAndHold(host, seed, withAppend(host, count), "append " + std::to_string(count));
      } else if (roll < 70) {
        // A ragged change only has to keep the geometry sound.
        std::size_t at = static_cast<std::size_t>(coin(host.rng)) % host.keys.size();
        host.keys = withRemove(host, at, 1);
        host.note("ragged remove at " + std::to_string(at));
        axisCommit(host, false);
        axisRest(host, seed);
      } else {
        double maxOffset = std::max(0.0, axisTotal(host.container) - host.windowSize);
        axisScrollTo(host, seed, static_cast<double>(coin(host.rng)) / 99.0 * maxOffset);
      }
      if (host.keys.empty()) {
        break;
      }
    }
  }
}

/*
 * Decoration rows, like date pills, are never the anchor. Changes that add or remove them
 * still hold the nearest content row.
 */
TEST(fuzz_non_anchorable_rows_never_hold_and_content_rows_do) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.inverted = coin(host.rng) < 50;
    open(host, seed, 30 + static_cast<std::size_t>(coin(host.rng) % 30));
    std::vector<std::string> pills;
    auto addPills = [&](std::vector<std::string> keys) {
      std::vector<std::string> next;
      for (std::size_t index = 0; index < keys.size(); ++index) {
        if (coin(host.rng) < 20) {
          std::string pill = "pill" + std::to_string(host.nextKey++);
          host.trueSizes[pill] = 28.0;
          pills.push_back(pill);
          next.push_back(pill);
        }
        next.push_back(keys[index]);
      }
      return next;
    };
    for (int step = 0; step < 8; ++step) {
      int roll = coin(host.rng);
      std::size_t count = 1 + static_cast<std::size_t>(coin(host.rng) % 6);
      std::vector<std::string> next;
      std::string what;
      if (roll < 40) {
        next = addPills(withPrepend(host, count));
        what = "prepend with pills";
      } else if (roll < 60) {
        next = addPills(withAppend(host, count));
        what = "append with pills";
      } else if (roll < 75 && !pills.empty()) {
        // Drop every pill, as an unread divider disappears.
        for (const std::string& key : host.keys) {
          if (key.rfind("pill", 0) != 0) {
            next.push_back(key);
          }
        }
        pills.clear();
        what = "drop pills";
      } else {
        double target = static_cast<double>(coin(host.rng)) / 99.0 * host.maxOffset();
        scrollTo(host, seed, target);
        continue;
      }
      // Pills are non anchorable from now on. Feed the list through the input each frame.
      struct PillGuard {
        SimHost& host;
        std::vector<std::string> saved;
      };
      std::vector<ScreenRow> before = visibleRows(host);
      host.keys = next;
      host.note(what + " n=" + std::to_string(next.size()));
      // Run the frames with the pill keys marked, the way the screen passes nonAnchorKeys.
      auto framed = [&](bool ownWrite) {
        if (!ownWrite) {
          host.stateOffset = host.hostOffset;
        }
        FrameInput frame = host.input(ownWrite);
        frame.nonAnchorKeys = pills;
        Virtualizer::update(host.container, frame);
        host.layoutPass();
        bool corrected = host.container.offsetCorrected;
        host.mountState();
        if (!ownWrite) {
          host.userScrolled = false;
        }
        return corrected;
      };
      framed(false);
      int settled = -1;
      for (int frame = 0; frame < MAX_SETTLE_FRAMES; ++frame) {
        bool corrected = framed(true);
        bool reported = framed(false);
        if (!corrected && !reported && !host.container.operation && !host.container.pendingScrollToEnd) {
          settled = frame;
          break;
        }
      }
      if (settled < 0) {
        failWith(host, seed, what + ": never settled");
      }
      checkFrame(host, seed, true);
      if (host.container.isAnchorable(host.container.anchor.key) == false && !host.container.anchor.key.empty() &&
          host.container.anchor.key.rfind("pill", 0) == 0) {
        // Only allowed when nothing else is on screen.
        bool contentVisible = false;
        for (const ScreenRow& row : visibleRows(host)) {
          if (row.key.rfind("pill", 0) != 0) {
            contentVisible = true;
          }
        }
        if (contentVisible) {
          failWith(host, seed, what + ": a pill became the anchor");
        }
      }
      for (const ScreenRow& row : before) {
        if (row.key.rfind("pill", 0) == 0) {
          continue;
        }
        double after = screenYOf(host, row.key);
        if (std::isnan(after)) {
          continue;
        }
        if (std::fabs(after - row.screenY) <= TOLERANCE) {
          break;
        }
        std::size_t index = host.container.indexOfKey(row.key);
        double wanted = host.container.revision.rows[index].offsetY - row.screenY;
        if ((wanted < 0.0 && host.hostOffset <= TOLERANCE) ||
            (wanted > host.maxOffset() && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE)) {
          break;
        }
        if (host.inverted && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE) {
          break;
        }
        failWith(host, seed, what + ": content row " + row.key + " moved from " + std::to_string(row.screenY) +
          " to " + std::to_string(after));
      }
    }
  }
}

/*
 * The window along the scroll axis resizes, like a keyboard or composer opening and closing.
 * An inverted list at its bottom stays at the bottom, one scrolled up keeps its row, and a
 * plain list keeps its row too. Nothing is left past the end.
 */
TEST(fuzz_window_resizes_keep_the_reader) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.inverted = coin(host.rng) < 60;
    open(host, seed, 30 + static_cast<std::size_t>(coin(host.rng) % 30));
    for (int step = 0; step < 8; ++step) {
      int roll = coin(host.rng);
      if (roll < 35) {
        double target = coin(host.rng) < 40 ? host.maxOffset() : static_cast<double>(coin(host.rng)) / 99.0 * host.maxOffset();
        scrollTo(host, seed, target);
        continue;
      }
      if (roll < 55) {
        changeAndHold(host, seed, withAppend(host, 1 + static_cast<std::size_t>(coin(host.rng) % 4)), "append");
        continue;
      }
      double previousWindow = host.windowSize;
      bool atBottom = host.inverted && host.hostOffset >= host.maxOffset() - INVERTED_FOLLOW_BAND &&
        !host.container.invertedBottomReleased;
      ScreenRow row = firstVisibleRow(host);
      host.windowSize = coin(host.rng) < 50 ? WINDOW_HEIGHT : WINDOW_HEIGHT - 300.0 - static_cast<double>(coin(host.rng));
      if (host.windowSize == previousWindow) {
        continue;
      }
      host.note("window " + std::to_string(previousWindow) + " -> " + std::to_string(host.windowSize) +
        " off=" + std::to_string(host.hostOffset) + (atBottom ? " at bottom" : ""));
      // The resize comes through the layout pass, like the Fabric host.
      host.commit(false);
      checkFrame(host, seed);
      if (host.settle() < 0) {
        failWith(host, seed, "window resize never settled");
      }
      checkFrame(host, seed, true);
      if (host.hostOffset > host.maxOffset() + TOLERANCE) {
        failWith(host, seed, "rests past the end after a resize: off=" + std::to_string(host.hostOffset) +
          " max=" + std::to_string(host.maxOffset()));
      }
      if (atBottom) {
        if (std::fabs(host.hostOffset - host.maxOffset()) > TOLERANCE) {
          failWith(host, seed, "inverted list left the bottom on a resize: off=" + std::to_string(host.hostOffset) +
            " max=" + std::to_string(host.maxOffset()));
        }
        continue;
      }
      if (row.key.empty()) {
        continue;
      }
      double after = screenYOf(host, row.key);
      if (std::isnan(after) || std::fabs(after - row.screenY) <= TOLERANCE) {
        continue;
      }
      std::size_t index = host.container.indexOfKey(row.key);
      double wanted = host.container.revision.rows[index].offsetY - row.screenY;
      if ((wanted < 0.0 && host.hostOffset <= TOLERANCE) ||
          (wanted > host.maxOffset() && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE)) {
        continue;
      }
      failWith(host, seed, "row " + row.key + " moved from " + std::to_string(row.screenY) + " to " + std::to_string(after) +
        " on a resize (host " + std::to_string(host.hostOffset) + ")");
    }
  }
}

/*
 * Predicted sizes arrive for rows not yet measured, some for keys not in the list yet. They
 * must never move the row on screen, always be replaced by the real measurement, and never
 * leave a stale prediction behind after invalidation.
 */
TEST(fuzz_predicted_sizes_never_move_the_reader) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.inverted = coin(host.rng) < 30;
    open(host, seed, 30 + static_cast<std::size_t>(coin(host.rng) % 30));
    for (int step = 0; step < 10; ++step) {
      int roll = coin(host.rng);
      if (roll < 25) {
        double target = static_cast<double>(coin(host.rng)) / 99.0 * host.maxOffset();
        scrollTo(host, seed, target);
        continue;
      }
      if (roll < 45) {
        // Predict the sizes of rows about to be prepended, then prepend them.
        std::size_t count = 1 + static_cast<std::size_t>(coin(host.rng) % 6);
        std::vector<std::string> next = withPrepend(host, count);
        for (std::size_t index = 0; index < count; ++index) {
          double guess = host.trueSizes[next[index]] + (coin(host.rng) < 50 ? 0.0 : 30.0);
          Virtualizer::applyPredictedRowSize(host.container, next[index], {WINDOW_WIDTH, guess});
        }
        changeAndHold(host, seed, next, "predicted prepend " + std::to_string(count));
        continue;
      }
      if (roll < 70) {
        // Predict sizes for existing unmeasured rows off screen.
        ScreenRow row = firstVisibleRow(host);
        // Predictions arrive before a frame and the frame's layout reflows them.
        for (std::size_t index = 0; index < host.keys.size(); ++index) {
          const Row& placed = host.container.revision.rows[index];
          if (placed.measured || coin(host.rng) < 50) {
            continue;
          }
          Virtualizer::applyPredictedRowSize(host.container, placed.key, {WINDOW_WIDTH, host.trueSizes[placed.key] + 10.0});
        }
        host.note("predict unmeasured rows off=" + std::to_string(host.hostOffset));
        host.commit(false);
        if (host.settle() < 0) {
          failWith(host, seed, "predictions never settled");
        }
        checkFrame(host, seed, true);
        if (!row.key.empty()) {
          double after = screenYOf(host, row.key);
          std::size_t index = host.container.indexOfKey(row.key);
          double wanted = host.container.revision.rows[index].offsetY - row.screenY;
          bool clamped = (wanted < 0.0 && host.hostOffset <= TOLERANCE) ||
            (wanted > host.maxOffset() && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE);
          bool bottomHeld = host.inverted && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE;
          if (!std::isnan(after) && std::fabs(after - row.screenY) > TOLERANCE && !clamped && !bottomHeld) {
            failWith(host, seed, "predictions moved row " + row.key + " from " + std::to_string(row.screenY) + " to " +
              std::to_string(after));
          }
        }
        continue;
      }
      if (roll < 85) {
        ScreenRow row = firstVisibleRow(host);
        host.note("invalidate predictions off=" + std::to_string(host.hostOffset));
        Virtualizer::invalidatePredictions(host.container);
        for (const Row& placed : host.container.revision.rows) {
          if (placed.predicted) {
            failWith(host, seed, "a prediction survived invalidation");
          }
        }
        host.commit(false);
        if (host.settle() < 0) {
          failWith(host, seed, "invalidation never settled");
        }
        checkFrame(host, seed, true);
        if (!row.key.empty()) {
          double after = screenYOf(host, row.key);
          std::size_t index = host.container.indexOfKey(row.key);
          double wanted = host.container.revision.rows[index].offsetY - row.screenY;
          bool clamped = (wanted < 0.0 && host.hostOffset <= TOLERANCE) ||
            (wanted > host.maxOffset() && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE);
          bool bottomHeld = host.inverted && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE;
          if (!std::isnan(after) && std::fabs(after - row.screenY) > TOLERANCE && !clamped && !bottomHeld) {
            failWith(host, seed, "invalidation moved row " + row.key + " from " + std::to_string(row.screenY) + " to " +
              std::to_string(after));
          }
        }
        continue;
      }
      changeAndHold(host, seed, withAppend(host, 1 + static_cast<std::size_t>(coin(host.rng) % 4)), "append");
    }
    // Every mounted row ends up measured, not predicted.
    for (const Row& row : host.container.revision.rows) {
      if (row.predicted && row.measured) {
        failWith(host, seed, "row " + row.key + " is both measured and predicted");
      }
    }
  }
}

namespace {

/*
 * Every row overlapping the host's viewport must be mounted: inside the core's window plus
 * the rows the host pads around it. Checked during motion, where checkCoverage is too strict.
 */
std::string checkMountedCoverage(const SimHost& host) {
  const Container& core = host.container;
  if (core.revision.rows.empty()) {
    return "";
  }
  auto visible = core.getMeasuredRange();
  if (visible.low == UNDEFINED_INDEX) {
    return "no window";
  }
  std::size_t low = visible.low;
  std::size_t high = visible.high;
  low = low > host.mountPad ? low - host.mountPad : 0;
  high += host.mountPad;
  for (std::size_t index = 0; index < core.revision.rows.size(); ++index) {
    const Row& row = core.revision.rows[index];
    if (row.offsetY + row.height <= host.hostOffset || row.offsetY >= host.hostOffset + host.windowSize) {
      continue;
    }
    if (index < low || index > high) {
      return "row " + std::to_string(index) + " on screen but not mounted [" + std::to_string(low) + ".." +
        std::to_string(high) + "] host=" + std::to_string(host.hostOffset);
    }
  }
  return "";
}

/*
 * The first row whose top is on screen that the reader also saw on screen last frame, at
 * previousOffset, with its real size. Only such a row can be seen to jump.
 */
ScreenRow firstRowStartingOnScreen(const SimHost& host, double previousOffset) {
  for (const Row& row : host.container.revision.rows) {
    double screenY = row.offsetY - host.hostOffset;
    bool sawIt = row.offsetY + row.height > previousOffset && row.offsetY < previousOffset + host.windowSize;
    if (screenY < 0.0 || !row.measured || !sawIt) {
      continue;
    }
    if (screenY >= host.windowSize) {
      break;
    }
    return {row.key, screenY};
  }
  return {};
}

}

/*
 * Long flings through unmeasured rows, in both directions, on plain and inverted lists.
 * Every frame mounts what is on screen and the content under the reader never jumps,
 * however far the measured rows are from the estimate.
 */
TEST(fuzz_flings_never_blank_or_jump) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.inverted = coin(host.rng) < 30;
    host.header = coin(host.rng) < 50 ? 0.0 : 120.0;
    open(host, seed, 300 + static_cast<std::size_t>(coin(host.rng)) * 4);
    scrollTo(host, seed, host.maxOffset() * static_cast<double>(coin(host.rng)) / 99.0);

    for (int fling = 0; fling < 4; ++fling) {
      double velocity = 1500.0 + static_cast<double>(coin(host.rng)) * 45.0;
      if (coin(host.rng) < 50) {
        velocity = -velocity;
      }
      host.note("fling v=" + std::to_string(velocity) + " from " + std::to_string(host.hostOffset));
      while (std::fabs(velocity) > 20.0) {
        double next = std::min(std::max(host.hostOffset + velocity, 0.0), host.maxOffset());
        bool clamped = next <= 0.0 || next >= host.maxOffset();
        double previousOffset = host.hostOffset;
        host.hostOffset = next;
        host.phase = ScrollPhase::Settling;
        host.userScrolled = true;
        ScreenRow held = firstRowStartingOnScreen(host, previousOffset);
        host.note("fling frame off=" + std::to_string(host.hostOffset) + " held=" + held.key);
        host.commit(false);
        checkFrame(host, seed);
        std::string coverage = checkMountedCoverage(host);
        if (!coverage.empty()) {
          failWith(host, seed, "blank during fling: " + coverage);
        }
        host.commit(true);
        checkFrame(host, seed);
        coverage = checkMountedCoverage(host);
        if (!coverage.empty()) {
          failWith(host, seed, "blank during fling after mount: " + coverage);
        }
        bool atEdge = host.hostOffset <= TOLERANCE || std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE;
        if (!held.key.empty() && !clamped && !atEdge) {
          double after = screenYOf(host, held.key);
          if (std::fabs(after - held.screenY) > TOLERANCE) {
            failWith(host, seed, "row " + held.key + " jumped from " + std::to_string(held.screenY) + " to " +
              std::to_string(after) + " mid fling");
          }
        }
        if (clamped) {
          break;
        }
        velocity *= 0.92;
      }
      host.phase = ScrollPhase::Idle;
      host.userScrolled = false;
      host.commit(false);
      rest(host, seed);
    }
  }
}

/*
 * A list starts small or empty, grows page by page as the reader reaches an edge, then
 * shrinks back to a few rows or none and grows again. It never rests outside its range,
 * a list shorter than the viewport rests at its start, and every arrival at an edge fires
 * its callback once per page.
 */
TEST(fuzz_lists_grow_from_small_and_shrink_back) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.inverted = coin(host.rng) < 40;
    host.followAppends = host.inverted && coin(host.rng) < 50;
    host.header = coin(host.rng) < 50 ? 0.0 : 60.0;
    int endReached = 0;
    int startReached = 0;
    std::size_t endReachedCount = UNDEFINED_INDEX;
    host.container.onEndReachedCallback = [&] {
      ++endReached;
      endReachedCount = host.container.revision.rows.size();
    };
    std::size_t startReachedCount = UNDEFINED_INDEX;
    host.container.onStartReachedCallback = [&] {
      ++startReached;
      startReachedCount = host.container.revision.rows.size();
    };
    open(host, seed, static_cast<std::size_t>(coin(host.rng) % 4));

    auto checkRange = [&](const std::string& what) {
      if (host.hostOffset < -TOLERANCE || host.hostOffset > host.maxOffset() + TOLERANCE) {
        failWith(host, seed, what + ": rests outside the range off=" + std::to_string(host.hostOffset) +
          " max=" + std::to_string(host.maxOffset()));
      }
      if (host.maxOffset() <= 0.0 && std::fabs(host.hostOffset) > TOLERANCE) {
        failWith(host, seed, what + ": a list shorter than the viewport rests at " + std::to_string(host.hostOffset));
      }
    };

    for (int step = 0; step < 14; ++step) {
      int roll = coin(host.rng);
      std::size_t page = 1 + static_cast<std::size_t>(coin(host.rng) % 15);
      if (roll < 30) {
        // The reader goes to the end. Arriving there fires once and loads the next page.
        int before = endReached;
        // Rows measured on the way move the end. Keep going until the reader rests on it.
        for (int attempt = 0; attempt < 8 && host.hostOffset < host.maxOffset() - TOLERANCE; ++attempt) {
          int beforeAttempt = endReached;
          scrollTo(host, seed, host.maxOffset());
          if (endReached - beforeAttempt > 1) {
            failWith(host, seed, "end reached fired " + std::to_string(endReached - beforeAttempt) + " times on one arrival");
          }
        }
        // Resting at the end of rows it never fired for means the reader waits for a page forever.
        bool atEnd = host.hostOffset >= host.maxOffset() - TOLERANCE;
        if (!host.keys.empty() && atEnd && endReachedCount != host.container.revision.rows.size()) {
          failWith(host, seed, "end reached never fired at the end of " + std::to_string(host.keys.size()) + " rows");
        }
        if (endReached > before) {
          changeAndHold(host, seed, withAppend(host, page), "load next page " + std::to_string(page));
          // A page that keeps the reader at the end fires again only after it changed the count.
          int afterPage = endReached;
          host.commit(false);
          rest(host, seed);
          if (endReached - afterPage > 1) {
            failWith(host, seed, "end reached fired repeatedly after a page");
          }
        }
      } else if (roll < 50) {
        int before = startReached;
        scrollTo(host, seed, 0.0);
        if (startReached - before > 1) {
          failWith(host, seed, "start reached fired " + std::to_string(startReached - before) + " times on one arrival");
        }
        // A list that scrolls, resting on its first row, has fired for the rows it has.
        bool scrollable = host.container.revision.contentHeight > host.windowSize;
        if (!host.keys.empty() && scrollable && host.hostOffset <= TOLERANCE &&
            startReachedCount != host.container.revision.rows.size()) {
          failWith(host, seed, "start reached never fired at the start of " + std::to_string(host.keys.size()) + " rows");
        }
        if (startReached > before) {
          changeAndHold(host, seed, withPrepend(host, page), "load previous page " + std::to_string(page));
        }
      } else if (roll < 65) {
        changeAndHold(host, seed, withAppend(host, page), "append " + std::to_string(page));
      } else if (roll < 80) {
        std::size_t keep = static_cast<std::size_t>(coin(host.rng) % 4);
        keep = std::min(keep, host.keys.size());
        bool fromStart = coin(host.rng) < 50;
        std::vector<std::string> next = fromStart
          ? std::vector<std::string>(host.keys.begin(), host.keys.begin() + static_cast<std::ptrdiff_t>(keep))
          : std::vector<std::string>(host.keys.end() - static_cast<std::ptrdiff_t>(keep), host.keys.end());
        host.keys = next;
        host.note(std::string("shrink to ") + std::to_string(keep) + (fromStart ? " from start" : " from end") +
          " off=" + std::to_string(host.hostOffset));
        host.commit(false);
        checkFrame(host, seed);
        rest(host, seed);
      } else {
        std::size_t count = 20 + static_cast<std::size_t>(coin(host.rng) % 60);
        changeAndHold(host, seed, withAppend(host, count), "grow by " + std::to_string(count));
      }
      checkRange("step " + std::to_string(step));
    }
  }
}

namespace {

/*
 * Every row overlapping the axis host's viewport is inside the core's window plus the host's pad.
 */
std::string checkAxisCoverage(const AxisHost& host, std::size_t pad) {
  const Container& core = host.container;
  if (core.revision.rows.empty()) {
    return "";
  }
  auto visible = core.getMeasuredRange();
  if (visible.low == UNDEFINED_INDEX) {
    return "no window";
  }
  std::size_t low = visible.low;
  std::size_t high = visible.high + pad;
  low = low > pad ? low - pad : 0;
  for (std::size_t index = 0; index < core.revision.rows.size(); ++index) {
    const Row& row = core.revision.rows[index];
    double start = core.horizontal ? row.offsetX : row.offsetY;
    double size = core.horizontal ? row.width : row.height;
    if (start + size <= host.hostOffset || start >= host.hostOffset + host.windowSize) {
      continue;
    }
    if (index < low || index > high) {
      return "row " + std::to_string(index) + " on screen but not mounted [" + std::to_string(low) + ".." +
        std::to_string(high) + "] host=" + std::to_string(host.hostOffset);
    }
  }
  return "";
}

}

/*
 * Flings through grids, masonry with uneven tracks, and horizontal lists, plain or inverted.
 * Every frame mounts what is on screen, and at rest the window alone covers it.
 */
TEST(fuzz_grid_and_horizontal_flings_never_blank) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    AxisHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    int shape = coin(host.rng) % 3;
    host.numberOfColumns = shape == 0 ? 2 + static_cast<std::size_t>(coin(host.rng) % 3) : 1;
    host.horizontal = shape == 1;
    host.inverted = shape == 2 || (shape == 1 && coin(host.rng) < 30);
    host.header = coin(host.rng) < 50 ? 0.0 : 80.0;
    axisOpen(host, seed, 150 + static_cast<std::size_t>(coin(host.rng)) * 3);
    double openedMax = std::max(0.0, axisTotal(host.container) - host.windowSize);
    if (host.inverted && std::fabs(host.hostOffset - openedMax) > TOLERANCE) {
      failWith(host, seed, "inverted list did not open at its end: off=" + std::to_string(host.hostOffset) +
        " max=" + std::to_string(openedMax));
    }
    for (int fling = 0; fling < 4; ++fling) {
      double velocity = 1200.0 + static_cast<double>(coin(host.rng)) * 40.0;
      if (coin(host.rng) < 50) {
        velocity = -velocity;
      }
      host.note("fling v=" + std::to_string(velocity) + " from " + std::to_string(host.hostOffset));
      while (std::fabs(velocity) > 20.0) {
        double maxOffset = std::max(0.0, axisTotal(host.container) - host.windowSize);
        double next = std::min(std::max(host.hostOffset + velocity, 0.0), maxOffset);
        host.hostOffset = next;
        host.phase = ScrollPhase::Settling;
        host.userScrolled = true;
        axisCommit(host, false);
        /*
         * Masonry tracks drift apart as rows get measured. A layout pass that shrinks one
         * track can pull more of it into view than the pad holds, until the next frame picks
         * the window again. Only single track lists are judged mid fling.
         */
        std::string coverage = host.numberOfColumns > 1 ? "" : checkAxisCoverage(host, host.mountPad);
        if (!coverage.empty()) {
          failWith(host, seed, "blank during fling: " + coverage);
        }
        axisCommit(host, true);
        if (next <= 0.0 || next >= maxOffset) {
          break;
        }
        velocity *= 0.9;
      }
      host.phase = ScrollPhase::Idle;
      host.userScrolled = false;
      axisRest(host, seed);
      std::string coverage = checkAxisCoverage(host, 0);
      if (!coverage.empty()) {
        failWith(host, seed, "blank at rest: " + coverage);
      }
      double maxOffset = std::max(0.0, axisTotal(host.container) - host.windowSize);
      if (host.hostOffset > maxOffset + TOLERANCE) {
        failWith(host, seed, "rests past the end after a fling");
      }
    }
  }
}

/*
 * Scroll commands on grids, masonry and horizontal lists land where they aim and settle,
 * with data changes in between.
 */
TEST(fuzz_grid_and_horizontal_scroll_commands_land) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    AxisHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    int shape = coin(host.rng) % 2;
    host.numberOfColumns = shape == 0 ? 2 + static_cast<std::size_t>(coin(host.rng) % 3) : 1;
    host.horizontal = shape == 1;
    host.header = coin(host.rng) < 50 ? 0.0 : 80.0;
    axisOpen(host, seed, 60 + static_cast<std::size_t>(coin(host.rng)) * 2);
    for (int step = 0; step < 8; ++step) {
      int roll = coin(host.rng);
      if (roll < 50) {
        std::size_t index = static_cast<std::size_t>(coin(host.rng)) % host.keys.size();
        double viewPosition = (coin(host.rng) % 3) * 0.5;
        std::string key = host.keys[index];
        host.note("scrollToRow " + std::to_string(index) + " pos " + std::to_string(viewPosition));
        host.container.scrollToRow(index, viewPosition);
        axisCommit(host, false);
        if (axisSettle(host) < 0) {
          failWith(host, seed, "scrollToRow never settled");
        }
        std::size_t landed = host.container.indexOfKey(key);
        const Row& row = host.container.revision.rows[landed];
        double start = host.horizontal ? row.offsetX : row.offsetY;
        double size = host.horizontal ? row.width : row.height;
        double freeSpace = host.windowSize - size;
        double wanted = start - (freeSpace > 0.0 ? viewPosition * freeSpace : 0.0);
        double maxOffset = std::max(0.0, axisTotal(host.container) - host.windowSize);
        double clampedWanted = std::min(std::max(wanted, 0.0), maxOffset);
        if (std::fabs(host.hostOffset - clampedWanted) > TOLERANCE) {
          failWith(host, seed, "scrollToRow landed at " + std::to_string(host.hostOffset) + " wanted " +
            std::to_string(clampedWanted));
        }
        std::string coverage = checkAxisCoverage(host, 0);
        if (!coverage.empty()) {
          failWith(host, seed, "blank after scrollToRow: " + coverage);
        }
      } else if (roll < 65) {
        host.note("scrollToEnd");
        host.container.scrollToEnd();
        axisCommit(host, false);
        if (axisSettle(host) < 0) {
          failWith(host, seed, "scrollToEnd never settled");
        }
        double maxOffset = std::max(0.0, axisTotal(host.container) - host.windowSize);
        if (std::fabs(host.hostOffset - maxOffset) > TOLERANCE) {
          failWith(host, seed, "scrollToEnd landed at " + std::to_string(host.hostOffset) + " max " + std::to_string(maxOffset));
        }
      } else if (roll < 80) {
        host.note("scrollToStart");
        host.container.scrollToStart();
        axisCommit(host, false);
        if (axisSettle(host) < 0) {
          failWith(host, seed, "scrollToStart never settled");
        }
        if (host.hostOffset > TOLERANCE) {
          failWith(host, seed, "scrollToStart landed at " + std::to_string(host.hostOffset));
        }
      } else {
        std::size_t count = host.numberOfColumns * (1 + static_cast<std::size_t>(coin(host.rng) % 4));
        axisChangeAndHold(host, seed, coin(host.rng) < 50 ? withPrepend(host, count) : withAppend(host, count),
          "change " + std::to_string(count));
      }
    }
  }
}

/*
 * An inverted chat whose header or footer resizes, like a loading spinner above the history
 * or a typing indicator below the newest row. The rows on screen hold, the same as for an
 * append. A footer growing at the bottom lands below the fold. Shrinking clamps to the end.
 */
TEST(fuzz_inverted_header_and_footer_changes_keep_the_reader) {
  for (std::uint32_t seed = 1; seed <= 200; ++seed) {
    SimHost host(seed);
    std::uniform_int_distribution<int> coin(0, 99);
    host.inverted = true;
    host.header = coin(host.rng) < 50 ? 0.0 : 60.0;
    open(host, seed, 20 + static_cast<std::size_t>(coin(host.rng) % 40));
    for (int step = 0; step < 10; ++step) {
      if (coin(host.rng) < 35) {
        double target = coin(host.rng) < 40 ? host.maxOffset() : static_cast<double>(coin(host.rng)) / 99.0 * host.maxOffset();
        scrollTo(host, seed, target);
        continue;
      }
      bool atBottom = host.hostOffset >= host.maxOffset() - INVERTED_FOLLOW_BAND && !host.container.invertedBottomReleased;
      bool openingPin = atBottom && host.container.invertedOpeningPin;
      ScreenRow row = firstVisibleRow(host);
      double offsetBefore = host.hostOffset;
      double previousHeader = host.header;
      bool footerChange = coin(host.rng) < 50;
      if (footerChange) {
        host.footer = host.footer > 0.0 ? 0.0 : 40.0 + static_cast<double>(coin(host.rng));
        host.note("footer -> " + std::to_string(host.footer) + (atBottom ? " at bottom" : ""));
      } else {
        host.header = host.header > 0.0 ? 0.0 : 60.0;
        host.note("header -> " + std::to_string(host.header) + (atBottom ? " at bottom" : ""));
      }
      host.commit(false);
      checkFrame(host, seed);
      if (host.settle() < 0) {
        failWith(host, seed, "change never settled");
      }
      checkFrame(host, seed, true);
      if (host.hostOffset > host.maxOffset() + TOLERANCE) {
        failWith(host, seed, "rests past the end");
      }
      /*
       * A chat still settling on the bottom it opened at keeps that bottom, and so does one
       * resting on its newest row.
       */
      if (openingPin || (atBottom && !row.key.empty() && row.key == host.keys.back())) {
        if (std::fabs(host.hostOffset - host.maxOffset()) > TOLERANCE) {
          failWith(host, seed, "left the opening bottom: off=" + std::to_string(host.hostOffset) +
            " max=" + std::to_string(host.maxOffset()));
        }
        continue;
      }
      if (row.key.empty()) {
        continue;
      }
      double after = screenYOf(host, row.key);
      bool headerOffScreen = offsetBefore > 0.0 && offsetBefore >= previousHeader;
      double expected = footerChange || headerOffScreen ? row.screenY : row.screenY + (host.header - previousHeader);
      std::size_t index = host.container.indexOfKey(row.key);
      double wanted = host.container.revision.rows[index].offsetY - expected;
      bool clamped = (wanted < 0.0 && host.hostOffset <= TOLERANCE) ||
        (wanted > host.maxOffset() && std::fabs(host.hostOffset - host.maxOffset()) <= TOLERANCE);
      if (std::fabs(after - expected) > TOLERANCE && !clamped) {
        failWith(host, seed, "row " + row.key + " at " + std::to_string(after) + " expected " + std::to_string(expected));
      }
    }
  }
}
