#include <shadowlist-core/host/ListUpdate.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

namespace azimgd::shadowlist {

namespace {

void sortUnique(std::vector<std::size_t>& indices) {
  std::sort(indices.begin(), indices.end());
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
}

/*
 * The splice a change makes in a list of size keys, its ends clipped to fit.
 */
KeySplice spliceOf(std::size_t size, const KeyChange& change) {
  std::size_t start = std::min(change.start, size);
  std::size_t end = std::min(change.end, size - start);
  return {start, size - start - end, change.inserted.size()};
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

bool BatchCollector::end() {
  if (depth_ == 0) {
    return false;
  }
  return --depth_ == 0;
}

void BatchCollector::addDeleted(const std::vector<std::size_t>& indices) {
  batch_.update.deleted.insert(batch_.update.deleted.end(), indices.begin(), indices.end());
}

void BatchCollector::addInserted(const std::vector<std::size_t>& indices) {
  batch_.update.inserted.insert(batch_.update.inserted.end(), indices.begin(), indices.end());
}

void BatchCollector::addMoved(std::size_t from, std::size_t to) {
  batch_.update.moved.push_back({from, to});
}

void BatchCollector::addReloaded(const std::vector<std::size_t>& indices) {
  batch_.update.reloaded.insert(batch_.update.reloaded.end(), indices.begin(), indices.end());
}

CollectedBatch BatchCollector::take() {
  CollectedBatch taken = std::move(batch_);
  batch_ = CollectedBatch{};
  return taken;
}

template <typename Changed>
void ContentVersions::keep(
  KeyList itemKeys,
  const std::vector<std::int64_t>& versions,
  Changed changed) {
  std::uint64_t round = ++round_;
  std::size_t count = std::min(itemKeys.size(), versions.size());
  std::size_t seen = 0;
  versions_.reserve(count);
  for (std::size_t item = 0; item < count; ++item) {
    auto [entry, inserted] = versions_.try_emplace(itemKeys[item], Entry{versions[item], round});
    if (inserted) {
      ++seen;
      continue;
    }
    // A key named twice keeps its first version.
    if (entry->second.round == round) {
      continue;
    }
    ++seen;
    if (entry->second.version != versions[item]) {
      changed(item);
    }
    entry->second = {versions[item], round};
  }
  if (seen == versions_.size()) {
    return;
  }
  for (auto entry = versions_.begin(); entry != versions_.end();) {
    entry = entry->second.round == round ? std::next(entry) : versions_.erase(entry);
  }
}

void ContentVersions::record(const std::vector<std::string>& itemKeys, const std::vector<std::int64_t>& versions) {
  record(KeyList(itemKeys), versions);
}

void ContentVersions::record(KeyList itemKeys, const std::vector<std::int64_t>& versions) {
  keep(itemKeys, versions, [](std::size_t) {});
}

std::vector<std::size_t> ContentVersions::update(
  const std::vector<std::string>& itemKeys,
  const std::vector<std::int64_t>& versions) {
  return update(KeyList(itemKeys), versions);
}

std::vector<std::size_t> ContentVersions::update(KeyList itemKeys, const std::vector<std::int64_t>& versions) {
  std::vector<std::size_t> changed;
  keep(itemKeys, versions, [&](std::size_t item) { changed.push_back(item); });
  return changed;
}

HeldKeysCommit HeldKeys::commit(KeyChange rows, std::optional<KeyChange> items, bool diff) {
  HeldKeysCommit result;
  result.rows = spliceOf(rows_.size(), rows);
  const SharedKeys& previousItems = sectioned_ ? items_ : rows_;
  const KeyChange& itemChange = items ? *items : rows;
  KeySplice itemSplice = spliceOf(previousItems.size(), itemChange);
  if (diff) {
    result.diff = diffKeys(previousItems.list(itemSplice.start, itemSplice.deleted), KeyList(itemChange.inserted));
    for (std::size_t& index : result.diff.deleted) {
      index += itemSplice.start;
    }
    for (std::size_t& index : result.diff.inserted) {
      index += itemSplice.start;
    }
    for (KeyMove& move : result.diff.moved) {
      move.from += itemSplice.start;
      move.to += itemSplice.start;
    }
  }
  // The items splice reads the rows held before when the list had no sections.
  items_ = items ? previousItems.splice(itemSplice.start, itemSplice.deleted, std::move(items->inserted)) : SharedKeys();
  sectioned_ = items.has_value();
  rows_ = rows_.splice(result.rows.start, result.rows.deleted, std::move(rows.inserted));
  return result;
}

void HeldKeys::insertRows(const std::vector<std::size_t>& indices, std::vector<std::string> keys) {
  std::size_t count = std::min(indices.size(), keys.size());
  if (count == 0) {
    return;
  }
  std::size_t size = rows_.size();
  SharedKeys::Builder builder(rows_, size + count);
  std::size_t from = 0;
  for (std::size_t at = 0; at < count; ++at) {
    // A position past the end goes at the end.
    std::size_t kept = std::min(indices[at] - std::min(indices[at], from + at), size - from);
    builder.append(from, kept);
    from += kept;
    builder.append(std::move(keys[at]));
  }
  builder.append(from, size - from);
  rows_ = builder.finish();
  sectioned_ = false;
  items_ = SharedKeys();
}

void HeldKeys::deleteRows(const std::vector<std::size_t>& indices) {
  std::vector<std::size_t> sorted = deletionIndices(indices, rows_.size());
  if (sorted.empty()) {
    return;
  }
  std::size_t size = rows_.size();
  SharedKeys::Builder builder(rows_, size - sorted.size());
  std::size_t from = 0;
  for (std::size_t index : sorted) {
    builder.append(from, index - from);
    from = index + 1;
  }
  builder.append(from, size - from);
  rows_ = builder.finish();
  sectioned_ = false;
  items_ = SharedKeys();
}

}
