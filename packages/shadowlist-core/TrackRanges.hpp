#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

namespace azimgd::shadowlist {

/*
 * An interval summary. Mixed tracks need actual geometry, not index modulo columns, to find
 * rows overlapping a window. Two scalars per node avoid a per-row track history matrix.
 */
struct TrackRange {
  double leading = std::numeric_limits<double>::infinity();
  double end = -std::numeric_limits<double>::infinity();
};

class TrackRanges final {
public:
  /*
   * Refresh only the placed suffix and its ancestors. Capacity grows geometrically on append;
   * removing rows clears their leaves without rebuilding the unchanged prefix.
   */
  template <typename RowAt>
  void update(std::size_t from, std::size_t count, RowAt rowAt) {
    if (count == 0 && size_ == 0) {
      return;
    }
    std::size_t previous = size_;
    if (count > capacity_) {
      capacity_ = std::max<std::size_t>(1, capacity_);
      while (capacity_ < count) {
        capacity_ *= 2;
      }
      nodes_.assign(capacity_ * 2, {});
      from = 0;
      previous = 0;
    }
    std::size_t through = std::max(previous, count);
    from = std::min(from, count);
    for (std::size_t index = from; index < count; ++index) {
      nodes_[capacity_ + index] = rowAt(index);
    }
    for (std::size_t index = count; index < previous; ++index) {
      nodes_[capacity_ + index] = {};
    }
    size_ = count;
    if (from >= through) {
      return;
    }
    std::size_t low = (capacity_ + from) / 2;
    std::size_t high = (capacity_ + through - 1) / 2;
    for (; low > 0; low /= 2, high /= 2) {
      for (std::size_t index = low; index <= high; ++index) {
        nodes_[index] = {
          std::min(nodes_[index * 2].leading, nodes_[index * 2 + 1].leading),
          std::max(nodes_[index * 2].end, nodes_[index * 2 + 1].end)};
      }
    }
  }

  /*
   * Visit actual overlaps in row order. Return inspected nodes so focused tests can bound
   * the search cost when one early tall row outlives thousands of rows on other tracks.
   */
  template <typename Visit>
  std::size_t visit(double lower, double upper, Visit&& callback) const {
    if (size_ == 0) {
      return 0;
    }
    return visitNode(1, lower, upper, callback);
  }

private:
  template <typename Visit>
  std::size_t visitNode(std::size_t node, double lower, double upper, Visit& callback) const {
    const auto& range = nodes_[node];
    if (range.end <= lower || range.leading > upper) {
      return 1;
    }
    if (node >= capacity_) {
      callback(node - capacity_);
      return 1;
    }
    return 1 + visitNode(node * 2, lower, upper, callback) + visitNode(node * 2 + 1, lower, upper, callback);
  }

  std::vector<TrackRange> nodes_;
  std::size_t capacity_ = 0;
  std::size_t size_ = 0;
};

}
