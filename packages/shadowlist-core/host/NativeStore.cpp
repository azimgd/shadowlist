#include <shadowlist-core/host/NativeStore.hpp>

#include <cstdlib>
#include <unordered_set>

namespace azimgd::shadowlist {

NativeStore::NativeStore() : keys_(std::make_shared<const std::vector<std::string>>()) {}

void NativeStore::rebuildIndex() {
  keyIndex_.clear();
  keyIndex_.reserve(rows_.size());
  for (std::size_t index = 0; index < rows_.size(); ++index) {
    keyIndex_.emplace(rows_[index].key, index);
  }
}

void NativeStore::publishKeys() {
  rebuildIndex();
  auto keys = std::make_shared<std::vector<std::string>>();
  keys->reserve(rows_.size());
  for (const auto& row : rows_) {
    keys->push_back(row.key);
  }
  keys_ = std::move(keys);
  ++keysVersion_;
}

bool NativeStore::setData(
  const JsonValue& items,
  const std::vector<std::string>& keys,
  const std::vector<std::string>& templates) {
  bool wasIndexed = indexed_;
  if (indexed_) {
    indexed_ = false;
    indexedCount_ = 0;
    indexedOrder_.clear();
    indexedIds_.clear();
    idIndex_.clear();
    indexedExtras_.clear();
  }
  std::unordered_map<std::string, NativeRow> previous;
  previous.reserve(rows_.size());
  for (auto& row : rows_) {
    previous.emplace(row.key, std::move(row));
  }

  std::vector<NativeRow> next;
  std::size_t count = items.isArray() ? std::min(items.size(), keys.size()) : 0;
  next.reserve(count);
  std::unordered_set<std::string> seen;
  seen.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const auto& key = keys[index];
    if (!seen.insert(key).second) {
      continue;
    }
    const std::string& templateName = index < templates.size() ? templates[index] : std::string{};
    auto found = previous.find(key);
    if (found != previous.end() && found->second.templateName == templateName && found->second.item == items[index]) {
      next.push_back(std::move(found->second));
      continue;
    }
    next.push_back(NativeRow{key, items[index], templateName, nextRowVersion_++});
  }

  // Leaving indexed mode always publishes, since the old keys belong to no stored row.
  bool sameKeys = !wasIndexed && next.size() == rows_.size() &&
    std::equal(next.begin(), next.end(), keys_->begin(), [](const NativeRow& row, const std::string& key) { return row.key == key; });
  rows_ = std::move(next);
  if (sameKeys) {
    rebuildIndex();
    return false;
  }
  publishKeys();
  return true;
}

bool NativeStore::setIndexed(
  std::size_t count,
  std::vector<std::int32_t> order,
  const std::string& indexField,
  const std::string& valueField,
  const std::vector<std::size_t>& extraIndices,
  const JsonValue& extraItems,
  const std::vector<std::string>& extraTemplates,
  std::vector<std::int32_t> ids) {
  bool wasIndexed = indexed_;
  if (!ids.empty() && ids.size() != count) {
    ids.clear();
  }
  if (!wasIndexed) {
    rows_.clear();
    keyIndex_.clear();
    indexed_ = true;
  }
  if (!order.empty() && order.size() != count) {
    order.clear();
  }
  bool idsChanged = !wasIndexed || ids != indexedIds_;
  bool orderChanged = idsChanged || indexField != indexField_ || valueField != valueField_ || order != indexedOrder_;
  indexField_ = indexField;
  valueField_ = valueField;
  indexedOrder_ = std::move(order);
  if (orderChanged) {
    indexedEpoch_ = nextRowVersion_++;
  }

  // Extra data that didn't change, with the same order, keeps its version so the row isn't rebound.
  std::unordered_map<std::size_t, IndexedExtra> extras;
  std::size_t extraCount = extraItems.isArray() ? std::min(extraItems.size(), extraIndices.size()) : 0;
  extras.reserve(extraCount);
  for (std::size_t entry = 0; entry < extraCount; ++entry) {
    std::size_t index = extraIndices[entry];
    if (index >= count) {
      continue;
    }
    IndexedExtra extra;
    extra.item = extraItems[entry].isObject() ? extraItems[entry] : JsonValue::object();
    extra.templateName = entry < extraTemplates.size() ? extraTemplates[entry] : std::string{};
    auto previous = indexedExtras_.find(index);
    extra.version = !orderChanged && previous != indexedExtras_.end() && previous->second.item == extra.item &&
        previous->second.templateName == extra.templateName
      ? previous->second.version
      : nextRowVersion_++;
    extras[index] = std::move(extra);
  }
  // A row that lost its extra data gets a new shared version so it doesn't match its old one.
  if (!orderChanged) {
    for (const auto& [index, previous] : indexedExtras_) {
      if (extras.find(index) == extras.end()) {
        indexedEpoch_ = std::max(indexedEpoch_, nextRowVersion_++);
        break;
      }
    }
  }
  indexedExtras_ = std::move(extras);

  bool positional = ids.empty();
  bool wasPositional = indexedIds_.empty();
  if (wasIndexed && !idsChanged && count == indexedCount_) {
    return false;
  }
  auto keys = std::make_shared<std::vector<std::string>>();
  keys->reserve(count);
  if (positional) {
    // Position keys only change at the end, so keep the ones still valid.
    std::size_t reuse = wasIndexed && wasPositional ? std::min(count, keys_->size()) : 0;
    keys->insert(keys->end(), keys_->begin(), keys_->begin() + static_cast<std::ptrdiff_t>(reuse));
    for (std::size_t index = reuse; index < count; ++index) {
      keys->push_back(std::to_string(index));
    }
    idIndex_.clear();
  } else {
    idIndex_.clear();
    idIndex_.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      // A repeated id keeps its first row. Later ones get a key the core won't find.
      if (idIndex_.emplace(ids[index], index).second) {
        keys->push_back(std::to_string(ids[index]));
      } else {
        keys->push_back("#dup" + std::to_string(index));
      }
    }
  }
  indexedIds_ = std::move(ids);
  keys_ = std::move(keys);
  indexedCount_ = count;
  ++keysVersion_;
  return true;
}

bool NativeStore::insertItems(
  std::size_t index,
  const JsonValue& items,
  const std::vector<std::string>& keys,
  const std::vector<std::string>& templates) {
  if (indexed_) {
    return false;
  }
  std::size_t count = items.isArray() ? std::min(items.size(), keys.size()) : 0;
  std::vector<NativeRow> inserted;
  inserted.reserve(count);
  std::unordered_set<std::string> seen;
  for (std::size_t itemIndex = 0; itemIndex < count; ++itemIndex) {
    const auto& key = keys[itemIndex];
    if (keyIndex_.find(key) != keyIndex_.end() || !seen.insert(key).second) {
      continue;
    }
    const std::string& templateName = itemIndex < templates.size() ? templates[itemIndex] : std::string{};
    inserted.push_back(NativeRow{key, items[itemIndex], templateName, nextRowVersion_++});
  }
  if (inserted.empty()) {
    return false;
  }
  std::size_t at = std::min(index, rows_.size());
  rows_.insert(
    rows_.begin() + static_cast<std::ptrdiff_t>(at),
    std::make_move_iterator(inserted.begin()),
    std::make_move_iterator(inserted.end()));
  publishKeys();
  return true;
}

bool NativeStore::updateItem(const std::string& key, const JsonValue& patch, const std::string& templateName, bool replace) {
  if (indexed_) {
    auto index = indexOf(key);
    if (!index) {
      return false;
    }
    auto& extra = indexedExtras_[*index];
    if (replace) {
      extra.item = patch.isObject() ? patch : JsonValue::object();
    } else if (patch.isObject()) {
      for (const auto& [field, value] : patch.items()) {
        if (value.isNull()) {
          extra.item.erase(field);
        } else {
          extra.item[field] = value;
        }
      }
    }
    if (!templateName.empty()) {
      extra.templateName = templateName;
    }
    extra.version = nextRowVersion_++;
    if (extra.item.empty() && extra.templateName.empty()) {
      // Back to a plain row. The shared version is older, so it still rebinds.
      indexedExtras_.erase(*index);
    }
    return true;
  }
  auto found = keyIndex_.find(key);
  if (found == keyIndex_.end()) {
    return false;
  }
  NativeRow& row = rows_[found->second];
  if (replace || !row.item.isObject() || !patch.isObject()) {
    row.item = patch;
  } else {
    for (const auto& [field, value] : patch.items()) {
      row.item[field] = value;
    }
  }
  if (!templateName.empty()) {
    row.templateName = templateName;
  }
  row.version = nextRowVersion_++;
  return true;
}

bool NativeStore::removeItems(const std::vector<std::string>& keys) {
  if (indexed_) {
    return false;
  }
  std::unordered_set<std::string> doomed(keys.begin(), keys.end());
  auto end = std::remove_if(rows_.begin(), rows_.end(), [&](const NativeRow& row) { return doomed.count(row.key) > 0; });
  if (end == rows_.end()) {
    return false;
  }
  rows_.erase(end, rows_.end());
  publishKeys();
  return true;
}

NativeStore::MoveResult NativeStore::moveItem(const std::string& key, std::size_t toIndex) {
  if (indexed_) {
    return MoveResult::Missing;
  }
  auto found = keyIndex_.find(key);
  if (found == keyIndex_.end()) {
    return MoveResult::Missing;
  }
  std::size_t from = found->second;
  std::size_t to = std::min(toIndex, rows_.size() - 1);
  if (from == to) {
    return MoveResult::Unchanged;
  }
  NativeRow row = std::move(rows_[from]);
  rows_.erase(rows_.begin() + static_cast<std::ptrdiff_t>(from));
  rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(to), std::move(row));
  publishKeys();
  return MoveResult::Moved;
}

std::optional<std::size_t> NativeStore::indexOf(const std::string& key) const {
  if (!indexed_) {
    auto found = keyIndex_.find(key);
    return found == keyIndex_.end() ? std::nullopt : std::optional<std::size_t>(found->second);
  }
  if (!indexedIds_.empty()) {
    char* end = nullptr;
    long id = std::strtol(key.c_str(), &end, 10);
    if (key.empty() || *end != '\0' || id < INT32_MIN || id > INT32_MAX) {
      return std::nullopt;
    }
    auto found = idIndex_.find(static_cast<std::int32_t>(id));
    return found == idIndex_.end() ? std::nullopt : std::optional<std::size_t>(found->second);
  }
  if (key.empty() || key.size() > 18) {
    return std::nullopt;
  }
  std::size_t index = 0;
  for (char character : key) {
    if (character < '0' || character > '9') {
      return std::nullopt;
    }
    index = index * 10 + static_cast<std::size_t>(character - '0');
  }
  // A key with a leading zero is not a row.
  if (key.size() > 1 && key.front() == '0') {
    return std::nullopt;
  }
  return index < indexedCount_ ? std::optional<std::size_t>(index) : std::nullopt;
}

const NativeRow* NativeStore::find(const std::string& key, NativeRow& scratch, std::size_t& position) const {
  if (!indexed_) {
    auto found = keyIndex_.find(key);
    if (found == keyIndex_.end()) {
      return nullptr;
    }
    position = found->second;
    return &rows_[found->second];
  }
  auto index = indexOf(key);
  if (!index) {
    return nullptr;
  }
  position = *index;
  auto extra = indexedExtras_.find(*index);
  scratch.key = key;
  scratch.templateName = extra != indexedExtras_.end() ? extra->second.templateName : std::string{};
  scratch.version = extra != indexedExtras_.end() ? extra->second.version : indexedEpoch_;
  scratch.item = JsonValue();
  return &scratch;
}

JsonValue NativeStore::indexedItem(std::size_t position) const {
  auto extra = indexedExtras_.find(position);
  JsonValue item = extra != indexedExtras_.end() ? extra->second.item : JsonValue::object();
  if (!indexField_.empty()) {
    item[indexField_] = static_cast<std::int64_t>(position);
  }
  if (!valueField_.empty()) {
    item[valueField_] = indexedOrder_.empty() ? static_cast<std::int64_t>(position)
                                              : static_cast<std::int64_t>(indexedOrder_[position]);
  }
  return item;
}

JsonValue NativeStore::item(const std::string& key) const {
  auto index = indexOf(key);
  if (!index) {
    return nullptr;
  }
  return indexed_ ? indexedItem(*index) : rows_[*index].item;
}

}
