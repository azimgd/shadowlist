#include <shadowlist-core/host/TreeRows.hpp>

#include <iterator>
#include <utility>

namespace azimgd::shadowlist {

namespace {

std::uint8_t flagsOf(const TreeRow& row) {
  return static_cast<std::uint8_t>((row.hasChildren ? TREE_ROW_HAS_CHILDREN : 0) |
    (row.hasChildren && row.expanded ? TREE_ROW_EXPANDED : 0));
}

}

void TreeRows::reset(std::vector<TreeRow> rows) {
  keys_.clear();
  depths_.clear();
  flags_.clear();
  keys_.reserve(rows.size());
  depths_.reserve(rows.size());
  flags_.reserve(rows.size());
  for (TreeRow& row : rows) {
    depths_.push_back(row.depth);
    flags_.push_back(flagsOf(row));
    keys_.push_back(std::move(row.key));
  }
}

TreeRow TreeRows::rowAt(std::size_t row) const {
  if (row >= keys_.size()) {
    return {};
  }
  return {keys_[row], depths_[row], (flags_[row] & TREE_ROW_HAS_CHILDREN) != 0, (flags_[row] & TREE_ROW_EXPANDED) != 0};
}

std::size_t TreeRows::subtreeEnd(std::size_t row) const {
  if (row >= keys_.size()) {
    return keys_.size();
  }
  std::uint32_t depth = depths_[row];
  std::size_t end = row + 1;
  while (end < keys_.size() && depths_[end] > depth) {
    ++end;
  }
  return end;
}

TreeSplice TreeRows::expand(std::size_t row, std::vector<TreeRow> descendants) {
  if (row >= keys_.size() || (flags_[row] & TREE_ROW_HAS_CHILDREN) == 0) {
    return {row >= keys_.size() ? keys_.size() : row + 1, 0, 0};
  }
  std::size_t start = row + 1;
  std::size_t end = subtreeEnd(row);
  flags_[row] |= TREE_ROW_EXPANDED;
  auto first = static_cast<std::ptrdiff_t>(start);
  auto last = static_cast<std::ptrdiff_t>(end);
  keys_.erase(keys_.begin() + first, keys_.begin() + last);
  depths_.erase(depths_.begin() + first, depths_.begin() + last);
  flags_.erase(flags_.begin() + first, flags_.begin() + last);
  std::vector<std::string> keys;
  std::vector<std::uint32_t> depths;
  std::vector<std::uint8_t> flags;
  keys.reserve(descendants.size());
  depths.reserve(descendants.size());
  flags.reserve(descendants.size());
  for (TreeRow& descendant : descendants) {
    depths.push_back(descendant.depth);
    flags.push_back(flagsOf(descendant));
    keys.push_back(std::move(descendant.key));
  }
  keys_.insert(keys_.begin() + first, std::make_move_iterator(keys.begin()), std::make_move_iterator(keys.end()));
  depths_.insert(depths_.begin() + first, depths.begin(), depths.end());
  flags_.insert(flags_.begin() + first, flags.begin(), flags.end());
  return {start, end - start, keys.size()};
}

TreeSplice TreeRows::collapse(std::size_t row) {
  if (row >= keys_.size()) {
    return {keys_.size(), 0, 0};
  }
  std::size_t start = row + 1;
  std::size_t end = subtreeEnd(row);
  flags_[row] &= static_cast<std::uint8_t>(~TREE_ROW_EXPANDED);
  auto first = static_cast<std::ptrdiff_t>(start);
  auto last = static_cast<std::ptrdiff_t>(end);
  keys_.erase(keys_.begin() + first, keys_.begin() + last);
  depths_.erase(depths_.begin() + first, depths_.begin() + last);
  flags_.erase(flags_.begin() + first, flags_.begin() + last);
  return {start, end - start, 0};
}

}
