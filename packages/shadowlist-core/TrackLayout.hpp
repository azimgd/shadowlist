#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Public span input converted before placement. All hosts use one through columns; a full
 * span is columns. Non-finite input uses one, fractional input truncates toward zero.
 */
inline std::size_t itemSpanFrom(double value, std::size_t columns) {
  columns = std::max<std::size_t>(1, columns);
  if (!std::isfinite(value) || value <= 1.0) return 1;
  if (value >= static_cast<double>(columns)) return columns;
  return static_cast<std::size_t>(value);
}

struct TrackPlacement {
  std::size_t column = 0;
  std::size_t span = 1;
  double leading = 0.0;
  double extent = 0.0;
};

/*
 * Contiguous spans over the columns' stacks. The next row starts at the next column, wrapping
 * before a span that would overflow. A full span starts after every stack and advances them
 * together. The vectors retain their capacity when a drag or a reflow resets this scratch state.
 */
class TrackLayout final {
public:
  TrackLayout() = default;
  explicit TrackLayout(std::size_t columns, double leading = 0.0) { reset(columns, leading); }

  void reset(std::size_t columns, double leading = 0.0) {
    initialLeading_ = leading;
    trackEnds_.assign(std::max<std::size_t>(1, columns), leading);
    nextColumn_ = 0;
  }

  /*
   * Resume before a changed row from the already placed prefix. A mixed span can leave a track
   * unused for arbitrarily many rows: its end can be much earlier than the last columns rows.
   * Append can keep this layout's existing summary and call place directly, without this walk.
   */
  template <typename RowAt>
  void seed(std::size_t before, RowAt rowAt) {
    if (trackEnds_.empty()) reset(1);
    std::size_t columns = trackEnds_.size();
    std::fill(trackEnds_.begin(), trackEnds_.end(), initialLeading_);
    known_.assign(columns, 0);
    nextColumn_ = 0;
    if (before == 0) return;
    TrackPlacement previous = rowAt(before - 1);
    if (previous.column < columns && previous.span <= columns - previous.column) {
      nextColumn_ = previous.column + previous.span;
      if (nextColumn_ == columns) nextColumn_ = 0;
    }
    std::size_t remaining = columns;
    for (std::size_t index = before; index-- > 0 && remaining > 0;) {
      TrackPlacement row = rowAt(index);
      if (row.column >= columns) continue;
      std::size_t span = std::min(row.span, columns - row.column);
      for (std::size_t column = row.column; column < row.column + span; ++column) {
        if (!known_[column]) {
          known_[column] = 1;
          --remaining;
          trackEnds_[column] = row.leading + row.extent;
        }
      }
    }
  }

  TrackPlacement place(std::size_t span, double extent) {
    if (trackEnds_.empty()) reset(1);
    std::size_t columns = trackEnds_.size();
    span = std::clamp<std::size_t>(span, 1, columns);
    if (span > columns - nextColumn_) nextColumn_ = 0;
    auto first = trackEnds_.begin() + nextColumn_;
    double leading = *std::max_element(first, first + span);
    TrackPlacement placement{nextColumn_, span, leading, extent};
    std::fill(first, first + span, leading + extent);
    nextColumn_ += span;
    if (nextColumn_ == columns) nextColumn_ = 0;
    return placement;
  }

  const std::vector<double>& getTrackEnds() const { return trackEnds_; }
  std::size_t getNextColumn() const { return nextColumn_; }
  double getContentEnd() const {
    return trackEnds_.empty() ? initialLeading_ : *std::max_element(trackEnds_.begin(), trackEnds_.end());
  }

private:
  std::vector<double> trackEnds_;
  std::vector<unsigned char> known_;
  std::size_t nextColumn_ = 0;
  double initialLeading_ = 0.0;
};

}
