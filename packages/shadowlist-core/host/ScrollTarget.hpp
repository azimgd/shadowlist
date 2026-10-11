#pragma once

#include <shadowlist-core/Container.hpp>

#include <cstddef>
#include <optional>
#include <string>

namespace azimgd::shadowlist {

/*
 * A scroll position that survives data changes: the row at the viewport start, by key, and
 * how far the viewport start is past the row's leading edge.
 */
struct AnchorState {
  std::string key;
  double offset = 0.0;
};

/*
 * The row at the viewport start for an offset along the scroll axis, or nothing before the
 * first layout. The row nearest the viewport start that still reaches into it wins, and in a
 * grid the first column's row wins a tie.
 */
std::optional<AnchorState> anchorStateAt(const Container& core, double offset);

/*
 * Where a scroll to a row aims with the core's current sizes: the row placed at viewPosition
 * and moved viewOffset further, inside 0 and maxOffset. Unmeasured rows use their estimates.
 */
double rowTargetOffset(
  const Container& core,
  std::size_t index,
  double viewPosition,
  double viewOffset,
  double windowAlong,
  double maxOffset);

/*
 * Where a scroll command aims with the core's current sizes, for a host that animates toward
 * it before the core lands exactly. SCROLL_TO_END_INDEX aims at the end and SCROLL_TO_OFFSET_INDEX
 * at the offset in viewOffset. Returns nothing for an index past the rows.
 */
std::optional<double> commandTargetOffset(
  const Container& core,
  double commandIndex,
  double viewPosition,
  double viewOffset);

/*
 * Where a page scroll lands: one window toward the end for a positive direction, toward the
 * start for a negative one, inside 0 and maxOffset. Accessibility page scrolls use it.
 */
double pageScrollTarget(double offset, double windowAlong, double maxOffset, int direction);

}
