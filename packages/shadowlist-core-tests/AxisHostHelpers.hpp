#pragma once

#include "FuzzHelpers.hpp"
#include "SimHostHelpers.hpp"
#include "TestFramework.hpp"
#include "TestHelpers.hpp"
#include "VirtualizationHelpers.hpp"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

/*
 * AxisHost, the SimHost with a grid or a horizontal axis, and what the fuzz tests do to it.
 */
namespace slt {

using namespace azimgd::shadowlist;

/*
 * The faults of HostFaults the axis host has a model of.
 */
inline constexpr bool HostFaults::* AXIS_HOST_FAULTS[] = {
  &HostFaults::lateEchoes,
  &HostFaults::repeatedEchoes,
  &HostFaults::lateMeasurements,
  &HostFaults::lateMeasurementsOnScreen,
};

/*
 * The same host with a grid or a horizontal axis. Rows keep their scroll axis size in
 * trueSizes and the layout pass feeds it on the right axis. It writes a correction at once,
 * without ScrollSync, and refuses a fault outside AXIS_HOST_FAULTS.
 */
struct AxisHost : SimHost {
  std::size_t numberOfColumns = 1;
  bool horizontal = false;

  AxisHost(std::uint32_t seed, FuzzEpisode& episode, const HostFaults& faults = {}) : SimHost(seed, episode, faults) {
    for (const HostFault& fault : HOST_FAULTS) {
      bool modelled = std::find(std::begin(AXIS_HOST_FAULTS), std::end(AXIS_HOST_FAULTS), fault.flag) !=
        std::end(AXIS_HOST_FAULTS);
      if (faults.*fault.flag && !modelled) {
        fail(std::string("AxisHost has no model of the fault '") + fault.kind + "'");
      }
    }
  }

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

/*
 * One commit for the axis host: update, a layout pass that measures the mounted rows along
 * the scroll axis, then the idle mount of the state.
 */
inline bool axisCommit(AxisHost& host, bool ownWrite) {
  Container& core = host.container;
  if (!ownWrite) {
    host.stateOffset = host.hostOffset;
  }
  FrameInput frame = host.axisInput(ownWrite);
  Virtualizer::update(core, frame);
  if (host.rollRepeatedEcho()) {
    Virtualizer::update(core, frame);
  }
  host.measureMountedRows();

  bool corrected = core.state.offsetCorrected;
  ContainerStateUpdate state = core.resolveStateUpdate(host.stateOffset, host.stateOffset,
    core.revision.contentWidth, core.revision.contentHeight);
  double maxOffset = host.maxOffset();
  if (host.hostOffset > maxOffset) {
    host.hostOffset = maxOffset;
  }
  if (state.changed) {
    host.stateOffset = host.horizontal ? state.offsetX : state.offsetY;
  }
  if (state.applyOffset && !host.withholdWrite(state)) {
    host.echoedToken = state.commitToken;
    host.echoedRevision = state.commitRevision;
    double applied = std::min(std::max(host.horizontal ? state.offsetX : state.offsetY, 0.0), maxOffset);
    if (std::fabs(applied - host.hostOffset) >= 0.01) {
      host.hostOffset = applied;
    }
  }
  host.userScrolled = false;
  host.observeCommit();
  if (host.tracing) {
    IndexRange measured = core.getMeasuredRange();
    std::printf(
      "      %s off=%.1f core=%.1f total=%.1f max=%.1f n=%zu op=%s:%llu corrected=%d applied=%d anchor=%s sub=%.1f "
      "win=[%zd..%zd]\n",
      ownWrite ? "own " : "host", host.hostOffset, host.axisOffset(), host.axisTotal(), maxOffset,
      core.revision.rows.size(), core.state.operation ? operationKind(core.state.operation->type) : "none",
      static_cast<unsigned long long>(core.state.operation ? core.state.operation->commitToken : 0), corrected ? 1 : 0,
      state.applyOffset ? 1 : 0, core.state.anchor.key.c_str(), core.state.anchor.offset,
      static_cast<std::ptrdiff_t>(measured.low), static_cast<std::ptrdiff_t>(measured.high));
    if (core.state.operation) {
      std::printf("      target=%s sub=%.1f\n", core.state.operation->target.key.c_str(),
        core.state.operation->target.offset);
    }
  }
  return corrected;
}

inline int axisSettle(AxisHost& host) {
  for (int frame = 0; frame < FUZZ_MAX_SETTLE_FRAMES; ++frame) {
    bool corrected = axisCommit(host, true);
    bool reported = axisCommit(host, false);
    if (!corrected && !reported && !host.container.state.operation && !host.container.state.pendingScrollToEnd &&
        !host.measuresPending) {
      return frame;
    }
  }
  return -1;
}

/*
 * First anchorable row overlapping the viewport, in index order. In a grid the tracks have
 * independent heights. One offset can only hold one track: the core holds the first
 * overlapping row by index, and so does this check.
 */
inline ScreenRow firstAxisRow(const AxisHost& host) {
  const Container& core = host.container;
  auto onScreen = [&](std::size_t index) {
    double start = offsetOf(core, index);
    return start + sizeOf(core, index) > host.hostOffset && start < host.hostOffset + host.windowSize;
  };
  // A grid keeps holding the row it already holds while that row is on screen.
  if (host.numberOfColumns > 1 && !core.state.anchor.key.empty()) {
    std::size_t index = core.indexOfKey(core.state.anchor.key);
    if (index != UNDEFINED_INDEX && onScreen(index)) {
      return {core.state.anchor.key, offsetOf(core, index) - host.hostOffset};
    }
  }
  for (std::size_t index = 0; index < core.revision.rows.size(); ++index) {
    const std::string& key = core.revision.rows[index].key;
    if (onScreen(index) && core.isAnchorable(key)) {
      return {key, offsetOf(core, index) - host.hostOffset};
    }
  }
  return {};
}

inline void checkAxisFrame(const AxisHost& host) {
  std::string geometry = checkGeometry(host);
  if (!geometry.empty()) {
    host.failWith("geometry: " + geometry);
  }
}

inline void axisRest(AxisHost& host) {
  if (axisSettle(host) < 0) {
    host.failWith("correction never settled at rest");
  }
  checkAxisFrame(host);
}

inline void axisScrollTo(AxisHost& host, double offset) {
  offset = std::min(std::max(offset, 0.0), host.maxOffset());
  host.note("scroll to " + std::to_string(offset));
  host.hostOffset = offset;
  host.phase = ScrollPhase::Dragging;
  host.userScrolled = true;
  axisCommit(host, false);
  host.phase = ScrollPhase::Idle;
  host.userScrolled = false;
  axisCommit(host, false);
  axisRest(host);
}

/*
 * A data change on a grid or horizontal list holds the row at the top of the screen. Only
 * one track can be held in a grid. The check is on the anchor's own track.
 */
inline void axisChangeAndHold(AxisHost& host, const std::vector<std::string>& nextKeys, const std::string& what) {
  ScreenRow row = firstAxisRow(host);
  host.keys = nextKeys;
  host.note(what + " n=" + std::to_string(nextKeys.size()) + " off=" + std::to_string(host.hostOffset));
  axisCommit(host, false);
  if (axisSettle(host) < 0) {
    host.failWith(what + ": correction never settled");
  }
  checkAxisFrame(host);
  if (row.key.empty()) {
    return;
  }
  double after = screenOf(host, row.key);
  if (std::isnan(after) || std::fabs(after - row.screen) <= FUZZ_TOLERANCE) {
    return;
  }
  double wanted = offsetOf(host.container, host.container.indexOfKey(row.key)) - row.screen;
  if (isHoldClamped(wanted, host.hostOffset, host.maxOffset())) {
    return;
  }
  host.failWith(what + ": row " + row.key + " moved from " + std::to_string(row.screen) + " to " +
    std::to_string(after) + " (host " + std::to_string(host.hostOffset) + ", max " + std::to_string(host.maxOffset()) +
    ")");
}

inline void axisOpen(AxisHost& host, std::size_t count) {
  host.keys = host.makeKeys(count);
  host.note("open n=" + std::to_string(count) + " cols=" + std::to_string(host.numberOfColumns) +
    (host.horizontal ? " horizontal" : ""));
  FrameInput first = host.axisInput(false);
  first.windowWidth = 0.0;
  first.windowHeight = 0.0;
  Virtualizer::update(host.container, first);
  axisCommit(host, false);
  axisRest(host);
}

}
