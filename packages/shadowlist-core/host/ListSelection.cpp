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
  return true;
}

std::vector<std::size_t> ListSelection::indicesIn(const std::vector<std::string>& keys) const {
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
  if (keys_.empty()) {
    return false;
  }
  std::unordered_set<std::string> kept;
  for (const std::string& key : keys) {
    if (set_.count(key) > 0) {
      kept.insert(key);
    }
  }
  if (kept.size() == keys_.size()) {
    return false;
  }
  return retainWhere([&](const std::string& key) { return kept.count(key) > 0; });
}

}
