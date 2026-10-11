#include <shadowlist-core/host/ListUpdate.hpp>

#include <algorithm>

namespace azimgd::shadowlist {

namespace {

void sortUnique(std::vector<std::size_t>& indices) {
  std::sort(indices.begin(), indices.end());
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
}

}

std::vector<std::size_t> insertionIndices(std::vector<std::size_t> indices, std::size_t previousCount) {
  sortUnique(indices);
  for (std::size_t rank = 0; rank < indices.size(); ++rank) {
    indices[rank] = std::min(indices[rank], previousCount + rank);
  }
  return indices;
}

std::vector<std::size_t> deletionIndices(std::vector<std::size_t> indices, std::size_t previousCount) {
  indices.erase(std::remove_if(indices.begin(), indices.end(),
    [previousCount](std::size_t index) { return index >= previousCount; }), indices.end());
  sortUnique(indices);
  return indices;
}

KeySplice keySplice(const std::vector<std::string>& previous, const std::vector<std::string>& next) {
  std::size_t limit = std::min(previous.size(), next.size());
  std::size_t start = 0;
  while (start < limit && previous[start] == next[start]) {
    ++start;
  }
  std::size_t end = 0;
  while (end < limit - start && previous[previous.size() - 1 - end] == next[next.size() - 1 - end]) {
    ++end;
  }
  return {start, previous.size() - start - end, next.size() - start - end};
}

std::unordered_set<std::string> keysOfRows(
  const std::vector<std::string>& keys,
  const std::vector<std::size_t>& rows) {
  std::unordered_set<std::string> found;
  for (std::size_t row : rows) {
    if (row < keys.size()) {
      found.insert(keys[row]);
    }
  }
  return found;
}

std::vector<std::size_t> rowsOfKeys(
  const std::vector<std::string>& keys,
  const std::unordered_set<std::string>& wanted) {
  std::vector<std::size_t> rows;
  if (wanted.empty()) {
    return rows;
  }
  for (std::size_t row = 0; row < keys.size(); ++row) {
    if (wanted.count(keys[row]) > 0) {
      rows.push_back(row);
    }
  }
  return rows;
}

void ContentVersions::record(const std::vector<std::string>& itemKeys, const std::vector<std::int64_t>& versions) {
  versions_.clear();
  versions_.reserve(itemKeys.size());
  for (std::size_t item = 0; item < itemKeys.size() && item < versions.size(); ++item) {
    versions_.emplace(itemKeys[item], versions[item]);
  }
}

std::vector<std::size_t> ContentVersions::update(
  const std::vector<std::string>& itemKeys,
  const std::vector<std::int64_t>& versions) {
  std::vector<std::size_t> changed;
  for (std::size_t item = 0; item < itemKeys.size() && item < versions.size(); ++item) {
    auto known = versions_.find(itemKeys[item]);
    if (known != versions_.end() && known->second != versions[item]) {
      changed.push_back(item);
    }
  }
  record(itemKeys, versions);
  return changed;
}

}
