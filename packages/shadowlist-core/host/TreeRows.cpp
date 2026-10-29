#include <shadowlist-core/host/TreeRows.hpp>

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
  TreeRows base = std::move(*this);
  return expandFrom(base, row, std::move(descendants));
}

TreeSplice TreeRows::collapse(std::size_t row) {
  TreeRows base = std::move(*this);
  return collapseFrom(base, row);
}

TreeSplice TreeRows::expandFrom(const TreeRows& base, std::size_t row, std::vector<TreeRow> descendants) {
  if (row >= base.size() || (base.flags_[row] & TREE_ROW_HAS_CHILDREN) == 0) {
    *this = base;
    return {row >= base.size() ? base.size() : row + 1, 0, 0};
  }
  std::size_t start = row + 1;
  std::size_t end = base.subtreeEnd(row);
  spliceFrom(base, start, end, descendants);
  flags_[row] |= TREE_ROW_EXPANDED;
  return {start, end - start, descendants.size()};
}

TreeSplice TreeRows::collapseFrom(const TreeRows& base, std::size_t row) {
  if (row >= base.size()) {
    *this = base;
    return {base.size(), 0, 0};
  }
  std::size_t start = row + 1;
  std::size_t end = base.subtreeEnd(row);
  std::vector<TreeRow> none;
  spliceFrom(base, start, end, none);
  flags_[row] &= static_cast<std::uint8_t>(~TREE_ROW_EXPANDED);
  return {start, end - start, 0};
}

void TreeRows::spliceFrom(const TreeRows& base, std::size_t start, std::size_t end, std::vector<TreeRow>& inserted) {
  auto first = static_cast<std::ptrdiff_t>(start);
  auto last = static_cast<std::ptrdiff_t>(end);
  std::size_t count = start + inserted.size() + (base.size() - end);
  keys_.clear();
  depths_.clear();
  flags_.clear();
  keys_.reserve(count);
  depths_.reserve(count);
  flags_.reserve(count);
  keys_.insert(keys_.end(), base.keys_.begin(), base.keys_.begin() + first);
  depths_.insert(depths_.end(), base.depths_.begin(), base.depths_.begin() + first);
  flags_.insert(flags_.end(), base.flags_.begin(), base.flags_.begin() + first);
  for (TreeRow& row : inserted) {
    depths_.push_back(row.depth);
    flags_.push_back(flagsOf(row));
    keys_.push_back(std::move(row.key));
  }
  keys_.insert(keys_.end(), base.keys_.begin() + last, base.keys_.end());
  depths_.insert(depths_.end(), base.depths_.begin() + last, base.depths_.end());
  flags_.insert(flags_.end(), base.flags_.begin() + last, base.flags_.end());
}

}
