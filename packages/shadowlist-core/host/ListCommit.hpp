#pragma once

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/LiveScroll.hpp>

#include <cstdint>
#include <vector>

namespace azimgd::shadowlist {

/*
 * The scroll fields of a list's state: what the host reported and what the layout pass
 * published back. Stored as doubles like the rest of the state.
 */
struct ListScrollState {
  double offsetX = 0.0;
  double offsetY = 0.0;
  bool offsetEnabled = false;
  /*
   * The offset the core started from when it published a correction. The offset minus this
   * is the correction as a delta. A commit can mount frames after the report it was built on,
   * and a moving view has gone further by then, so hosts add the delta to the live offset
   * instead of writing the absolute offset. Only meaningful while offsetEnabled is set.
   */
  double baseX = 0.0;
  double baseY = 0.0;
  /*
   * The id of the pending correction. The core sends it with the offset and the host echoes
   * it back. Zero means no correction, or a report from the host.
   */
  double commitToken = 0.0;
  /*
   * The token of a scroll command issued in C++, like a ShadowListNative scrollToIndex, or 0.
   * A host mounting it stops momentum first, like its own scroll commands do.
   */
  double momentumYieldToken = 0.0;
  bool userScrolled = false;
  double scrollPhase = SCROLL_PHASE_IDLE;
  double totalWidth = 0.0;
  double totalHeight = 0.0;
};

ScrollPhase scrollPhaseFromReport(double scrollPhase);

/*
 * Fill the frame's host fields from the state. A ShadowListNative scroll command stops
 * momentum when the host mounts it, so its frame counts as idle, or the core would think the
 * fling drives the correction. A finger on the list is different: the drag stays and cancels
 * the command. Returns whether the frame yields momentum that way.
 */
bool applyHostScroll(FrameInput& input, const ListScrollState& state, bool engineScrollCommand);

/*
 * What the layout pass publishes for the scroll fields. Returns whether they changed.
 * A correction that continues one the state already carries keeps its first base, so the
 * host never applies the same part twice. engineYieldToken is the ShadowListNative command
 * token, whose frame tells the host to stop momentum and write the offset.
 */
bool publishStateUpdate(ListScrollState& state, const ContainerStateUpdate& update, std::uint64_t engineYieldToken);

/*
 * The band to publish, see OffsetBand. Rows hidden until the host echoes a correction and
 * text size predictions still being measured both need more commits, so the band stays empty
 * and the host sends every frame until they are done.
 */
OffsetBand publishedOffsetBand(const Container& core, bool bandEnabled, bool rowsConcealed, bool measuringSizeSpecs);

inline bool offsetBandPublished(double low, double high, const OffsetBand& band) {
  return low == band.low && high == band.high;
}

/*
 * Sticky indices from props, for the core. Negatives are dropped since the core wants valid
 * ascending indices.
 */
void stickyIndicesFromProps(const std::vector<int>& propIndices, std::vector<std::size_t>& indices);

}
