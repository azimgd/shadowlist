#include <shadowlist-core/host/TreeSource.hpp>

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace azimgd::shadowlist {

void TreeSource::reset(std::vector<TreeRow> rows) {
  source_ = std::move(rows);
  indices_.clear();
  for (std::size_t index = 0; index < source_.size(); ++index) {
    TreeRow& row = source_[index];
    if (row.expanded) {
      expanded_.insert(row.key);
    }
    indices_[row.key] = index;
  }
  rebuild();
}

void TreeSource::setExpandedKeys(const std::vector<std::string>& keys) {
  expanded_ = std::unordered_set<std::string>(keys.begin(), keys.end());
  rebuild();
}

std::vector<std::string> TreeSource::expandedKeys() const {
  std::vector<std::string> keys(expanded_.begin(), expanded_.end());
  std::sort(keys.begin(), keys.end());
  return keys;
}

void TreeSource::toggle(const std::string& key) {
  if (expanded_.count(key) != 0) {
    expanded_.erase(key);
  } else {
    expanded_.insert(key);
  }
  rebuild();
}

std::vector<TreeRow> TreeSource::visibleFrom(std::size_t start, std::uint32_t depth) const {
  std::vector<TreeRow> rows;
  for (std::size_t index = start; index < source_.size();) {
    const TreeRow& row = source_[index];
    if (row.depth < depth) {
      break;
    }
    rows.push_back(row);
    ++index;
    if (!row.expanded) {
      while (index < source_.size() && source_[index].depth > row.depth) {
        ++index;
      }
    }
  }
  return rows;
}

void TreeSource::rebuild() {
  for (TreeRow& row : source_) {
    row.expanded = row.hasChildren && expanded_.count(row.key) != 0;
  }
  visible_.reset(visibleFrom(0, 0));
}

TreeSplice TreeSource::expand(std::size_t row) {
  if (row >= visible_.size()) {
    return {visible_.size(), 0, 0};
  }
  TreeRow visible = visible_.rowAt(row);
  auto found = indices_.find(visible.key);
  if (row >= visible_.size() || found == indices_.end() || !visible.hasChildren) {
    return {row >= visible_.size() ? visible_.size() : row + 1, 0, 0};
  }
  TreeRow& source = source_[found->second];
  expanded_.insert(source.key);
  source.expanded = true;
  return visible_.expand(row, visibleFrom(found->second + 1, source.depth + 1));
}

TreeSplice TreeSource::collapse(std::size_t row) {
  TreeRow visible = visible_.rowAt(row);
  auto found = indices_.find(visible.key);
  if (row < visible_.size() && found != indices_.end()) {
    expanded_.erase(visible.key);
    source_[found->second].expanded = false;
  }
  return visible_.collapse(row);
}

}
