#include <shadowlist-core/host/ListSelection.hpp>

namespace azimgd::shadowlist {

void ListSelection::setMultiple(bool multiple) {
  multiple_ = multiple;
}

bool ListSelection::select(const std::string& key, std::vector<std::string>& deselected) {
  if (keys_.count(key) > 0) {
    return false;
  }
  if (!multiple_) {
    clear(deselected);
  }
  keys_.insert(key);
  return true;
}

bool ListSelection::deselect(const std::string& key) {
  return keys_.erase(key) > 0;
}

void ListSelection::clear(std::vector<std::string>& deselected) {
  deselected.insert(deselected.end(), keys_.begin(), keys_.end());
  keys_.clear();
}

std::vector<std::size_t> ListSelection::indicesIn(const std::vector<std::string>& keys) const {
  std::vector<std::size_t> indices;
  if (keys_.empty()) {
    return indices;
  }
  for (std::size_t index = 0; index < keys.size(); ++index) {
    if (keys_.count(keys[index]) > 0) {
      indices.push_back(index);
    }
  }
  return indices;
}

void ListSelection::retainKeys(const std::vector<std::string>& keys) {
  if (keys_.empty()) {
    return;
  }
  std::unordered_set<std::string> kept;
  for (const std::string& key : keys) {
    if (keys_.count(key) > 0) {
      kept.insert(key);
    }
  }
  keys_ = std::move(kept);
}

}
