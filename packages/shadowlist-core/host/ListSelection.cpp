#include <shadowlist-core/host/ListSelection.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

namespace azimgd::shadowlist {

void ListSelection::setMultiple(bool multiple) {
  multiple_ = multiple;
}

bool ListSelection::select(const std::string& key, std::vector<std::string>& deselected) {
  if (!multiple_) {
    return selectOnly(key, deselected);
  }
  if (!set_.insert(key).second) {
    return false;
  }
  keys_.push_back(key);
  return true;
}

bool ListSelection::selectOnly(const std::string& key, std::vector<std::string>& deselected) {
  if (keys_.size() == 1 && keys_[0] == key) {
    return false;
  }
  for (std::string& previous : keys_) {
    if (previous != key) {
      deselected.push_back(std::move(previous));
    }
  }
  keys_.clear();
  set_.clear();
  keys_.push_back(key);
  set_.insert(key);
  return true;
}

bool ListSelection::deselect(const std::string& key) {
  if (set_.erase(key) == 0) {
    return false;
  }
  keys_.erase(std::find(keys_.begin(), keys_.end(), key));
  return true;
}

void ListSelection::clear(std::vector<std::string>& deselected) {
  deselected.insert(deselected.end(), std::make_move_iterator(keys_.begin()), std::make_move_iterator(keys_.end()));
  keys_.clear();
  set_.clear();
  dropAnchor();
}

void ListSelection::setAnchor(const std::string& key) {
  hasAnchor_ = true;
  anchorKey_ = key;
  leadKey_ = key;
  range_.clear();
}

bool ListSelection::selectRange(const std::vector<std::string>& keys, std::vector<std::string>& deselected) {
  if (keys.empty()) {
    return false;
  }
  leadKey_ = keys.back();
  if (!multiple_) {
    range_.clear();
    return selectOnly(keys.back(), deselected);
  }
  std::unordered_set<std::string> next(keys.begin(), keys.end());
  bool changed = false;
  for (const std::string& previous : range_) {
    if (next.count(previous) == 0 && deselect(previous)) {
      deselected.push_back(previous);
      changed = true;
    }
  }
  for (const std::string& key : keys) {
    if (set_.insert(key).second) {
      keys_.push_back(key);
      changed = true;
    }
  }
  range_ = keys;
  return changed;
}

void ListSelection::dropAnchor() {
  hasAnchor_ = false;
  anchorKey_.clear();
  leadKey_.clear();
  range_.clear();
}

void ListSelection::retainRange() {
  range_.erase(
    std::remove_if(range_.begin(), range_.end(), [&](const std::string& key) { return set_.count(key) == 0; }),
    range_.end());
}

bool ListSelection::setKeys(const std::vector<std::string>& keys) {
  std::vector<std::string> next;
  std::unordered_set<std::string> nextSet;
  next.reserve(keys.size());
  nextSet.reserve(keys.size());
  for (const std::string& key : keys) {
    if (nextSet.insert(key).second) {
      next.push_back(key);
    }
  }
  if (next == keys_) {
    return false;
  }
  keys_ = std::move(next);
  set_ = std::move(nextSet);
  range_.clear();
  return true;
}

std::vector<std::size_t> ListSelection::indicesIn(const std::vector<std::string>& keys) const {
  return indicesIn(KeyList(keys));
}

std::vector<std::size_t> ListSelection::indicesIn(KeyList keys) const {
  std::vector<std::size_t> indices;
  if (keys_.empty()) {
    return indices;
  }
  for (std::size_t index = 0; index < keys.size(); ++index) {
    if (set_.count(keys[index]) > 0) {
      indices.push_back(index);
    }
  }
  return indices;
}

bool ListSelection::retainKeys(const std::vector<std::string>& keys) {
  return retainKeys(KeyList(keys));
}

bool ListSelection::retainKeys(KeyList keys) {
  if (keys_.empty() && !hasAnchor_) {
    return false;
  }
  std::unordered_set<std::string> kept;
  bool anchorKept = false;
  bool leadKept = false;
  for (const std::string& key : keys) {
    if (set_.count(key) > 0) {
      kept.insert(key);
    }
    anchorKept = anchorKept || (hasAnchor_ && key == anchorKey_);
    leadKept = leadKept || (hasAnchor_ && key == leadKey_);
  }
  if (hasAnchor_ && !(anchorKept && leadKept)) {
    dropAnchor();
  }
  if (kept.size() == keys_.size()) {
    return false;
  }
  // An anchor still here was found above.
  return retainWhere([&](const std::string& key) {
    return kept.count(key) > 0 || (hasAnchor_ && (key == anchorKey_ || key == leadKey_));
  });
}

}
