#pragma once

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>
#include <shadowlist-core/host/LiveScroll.hpp>

#include <cstdint>
#include <vector>

namespace azimgd::shadowlist {

/*
 * The scroll fields of a list's state: what the host reported and what the layout pass
 * published back. Stored as doubles like the rest of the state, except the commit token.
 */
struct ListScrollState {
  double offsetX = 0.0;
  double offsetY = 0.0;
  bool offsetEnabled = false;
  /*
   * The offset the core started from when it published a correction. A moving view adds
   * the delta to its live offset. Only meaningful while offsetEnabled is set.
   */
  double baseX = 0.0;
  double baseY = 0.0;
  /*
   * The id of the pending correction, echoed back by the host. Zero means no correction, or a
   * report from the host.
   */
  std::uint64_t commitToken = 0;
  bool userScrolled = false;
  double scrollPhase = SCROLL_PHASE_IDLE;
  double contentWidth = 0.0;
  double contentHeight = 0.0;
};

/*
 * Fill the frame's host fields from the state.
 */
void applyHostScroll(FrameInput& input, const ListScrollState& state);

/*
 * What the layout pass publishes for the scroll fields. Returns whether they changed. A
 * correction that continues the one in the state keeps its first base.
 */
bool publishStateUpdate(ListScrollState& state, const ContainerStateUpdate& update);

/*
 * The band to publish, see OffsetBand. Empty while rows are hidden or size specs are still
 * being measured, since both need more commits.
 */
OffsetBand publishedOffsetBand(const Container& core, bool bandEnabled, bool rowsConcealed, bool measuringSizeSpecs);

inline bool offsetBandPublished(double low, double high, const OffsetBand& band) {
  return low == band.low && high == band.high;
}

/*
 * Sticky indices from props, for the core, without negatives.
 */
void stickyIndicesFromProps(const std::vector<int>& propIndices, std::vector<std::size_t>& indices);

}
