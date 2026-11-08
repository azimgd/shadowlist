#pragma once

#include <shadowlist-core/Constants.hpp>

#include <cmath>
#include <cstddef>

namespace azimgd::shadowlist {

/*
 * Keyboard selection over the rows: which row an arrow, a page or a home or end key moves the
 * lead to, and how far a row must scroll to show. Every host with keyboard selection asks
 * these. canSelect(row) says whether a row takes the selection, like an item the delegate allows
 * and not a section header.
 */

/*
 * The selectable row step rows from the lead, or the next one past it by the same step. A grid
 * places its rows in its columns in turn, and a step of the column count stays in the lead's
 * column. Without a lead, UNDEFINED_INDEX, a step forward starts at the first row and a step back
 * at the last, one row at a time. Past either end there is none, UNDEFINED_INDEX.
 */
template <typename CanSelect>
std::size_t stepRow(std::size_t lead, long step, std::size_t count, CanSelect canSelect) {
  if (count == 0 || step == 0) {
    return UNDEFINED_INDEX;
  }
  long last = static_cast<long>(count) - 1;
  long direction = step > 0 ? 1 : -1;
  bool hasLead = lead != UNDEFINED_INDEX && lead < count;
  long row = hasLead ? static_cast<long>(lead) + step : (step > 0 ? 0 : last);
  long stride = hasLead ? step : direction;
  for (; row >= 0 && row <= last; row += stride) {
    if (canSelect(static_cast<std::size_t>(row))) {
      return static_cast<std::size_t>(row);
    }
  }
  return UNDEFINED_INDEX;
}

/*
 * The first selectable row, or the last one with end, or UNDEFINED_INDEX.
 */
template <typename CanSelect>
std::size_t endRow(bool end, std::size_t count, CanSelect canSelect) {
  return stepRow(UNDEFINED_INDEX, end ? -1 : 1, count, canSelect);
}

/*
 * The row a page key moves the lead to: the farthest row in direction whose leading edge is
 * within windowAlong of the lead's, stepping back toward the lead to a selectable one. Without a
 * lead it is the first or last selectable row. UNDEFINED_INDEX when no row besides the lead fits.
 * leadingAt(row) is a row's leading edge along the axis.
 */
template <typename LeadingAt, typename CanSelect>
std::size_t pageRow(
  std::size_t lead,
  int direction,
  std::size_t count,
  double windowAlong,
  LeadingAt leadingAt,
  CanSelect canSelect) {
  if (lead == UNDEFINED_INDEX || lead >= count) {
    return endRow(direction < 0, count, canSelect);
  }
  long step = direction < 0 ? -1 : 1;
  long last = static_cast<long>(count) - 1;
  double start = leadingAt(lead);
  long row = static_cast<long>(lead);
  while (row + step >= 0 && row + step <= last &&
         std::fabs(leadingAt(static_cast<std::size_t>(row + step)) - start) <= windowAlong) {
    row += step;
  }
  for (; row != static_cast<long>(lead); row -= step) {
    if (canSelect(static_cast<std::size_t>(row))) {
      return static_cast<std::size_t>(row);
    }
  }
  return UNDEFINED_INDEX;
}

/*
 * Where a row scrolls to show: None while it is fully in view, Start when it is above the
 * viewport start, End when it reaches past the viewport end. The view position follows: 0 at
 * the start, 1 at the end.
 */
enum class RevealEdge {
  None,
  Start,
  End,
};

inline RevealEdge revealEdge(double leading, double extent, double offset, double windowAlong) {
  if (leading < offset) {
    return RevealEdge::Start;
  }
  if (leading + extent > offset + windowAlong) {
    return RevealEdge::End;
  }
  return RevealEdge::None;
}

}
