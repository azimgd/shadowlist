#pragma once

#include <shadowlist-core/host/JsonValue.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace azimgd::shadowlist {

/*
 * One ShadowListNative row: its key, its data, the template it uses and a version that
 * changes whenever the row must be bound again.
 */
struct NativeRow {
  std::string key;
  JsonValue item;
  std::string templateName;
  std::uint64_t version = 0;
};

/*
 * The data of one ShadowListNative, without anything that builds views. Rows are either
 * stored, or indexed: keyed by position or id and built on demand from an order array plus
 * extra data for a few rows. Not thread safe, the owner locks.
 */
class NativeStore {
public:
  NativeStore();

  /*
   * Replaces all rows. Keys and templates line up with items, and empty templates means the
   * default one. Rows with an unchanged item and template keep their version. Returns whether
   * the key list changed.
   */
  bool setData(const JsonValue& items, const std::vector<std::string>& keys, const std::vector<std::string>& templates);

  /*
   * Switches to indexed rows. See ShadowListNativeEngine::setIndexed. Returns whether the
   * key list changed.
   */
  bool setIndexed(
    std::size_t count,
    std::vector<std::int32_t> order,
    const std::string& indexField,
    const std::string& valueField,
    const std::vector<std::size_t>& extraIndices,
    const JsonValue& extraItems,
    const std::vector<std::string>& extraTemplates,
    std::vector<std::int32_t> ids);

  /*
   * Inserts before index, or appends past the end. Keys that already exist are skipped.
   * Returns whether anything was inserted. Does nothing for indexed rows.
   */
  bool insertItems(
    std::size_t index,
    const JsonValue& items,
    const std::vector<std::string>& keys,
    const std::vector<std::string>& templates);

  /*
   * Merges patch into the row's item, or replaces it. Indexed rows merge into their extra
   * data, where a null field drops it. Returns false for an unknown key.
   */
  bool updateItem(const std::string& key, const JsonValue& patch, const std::string& templateName, bool replace);

  // Returns whether any row was removed. Does nothing for indexed rows.
  bool removeItems(const std::vector<std::string>& keys);

  enum class MoveResult { Missing, Unchanged, Moved };
  MoveResult moveItem(const std::string& key, std::size_t toIndex);

  std::optional<std::size_t> indexOf(const std::string& key) const;

  /*
   * The row for key with its template and version, or null. A stored row is returned as is.
   * An indexed row is written into scratch without its item, see indexedItem.
   */
  const NativeRow* find(const std::string& key, NativeRow& scratch, std::size_t& position) const;

  // The item of an indexed row at position: its extra data plus the index and value fields.
  JsonValue indexedItem(std::size_t position) const;

  // The row's item, or null for an unknown key.
  JsonValue item(const std::string& key) const;

  bool indexed() const { return indexed_; }
  std::size_t size() const { return indexed_ ? indexedCount_ : rows_.size(); }

  /*
   * Every row key in order. The list is never changed once published, so it can be shared.
   * The version goes up whenever it is replaced.
   */
  const std::shared_ptr<const std::vector<std::string>>& keys() const { return keys_; }
  std::uint64_t keysVersion() const { return keysVersion_; }

private:
  struct IndexedExtra {
    JsonValue item = JsonValue::object();
    std::string templateName;
    std::uint64_t version = 0;
  };

  void rebuildIndex();
  void publishKeys();

  std::vector<NativeRow> rows_;
  std::unordered_map<std::string, std::size_t> keyIndex_;

  // In indexed mode rows_ and keyIndex_ stay empty.
  bool indexed_ = false;
  std::size_t indexedCount_ = 0;
  std::vector<std::int32_t> indexedOrder_;
  // Row ids used as keys, empty when rows are keyed by position.
  std::vector<std::int32_t> indexedIds_;
  std::unordered_map<std::int32_t, std::size_t> idIndex_;
  std::string indexField_;
  std::string valueField_;
  std::unordered_map<std::size_t, IndexedExtra> indexedExtras_;
  // Version of every row without extra data. A new order bumps it.
  std::uint64_t indexedEpoch_ = 0;

  std::shared_ptr<const std::vector<std::string>> keys_;
  std::uint64_t keysVersion_ = 1;
  std::uint64_t nextRowVersion_ = 1;
};

/*
 * Which cached rows to drop so at most keep idle ones stay. Rows used at now are in use and
 * never dropped. The idle rows used longest ago go first. usedAt maps an entry to its clock.
 */
template <typename Map, typename UsedAt>
std::vector<typename Map::key_type> staleNativeRows(const Map& entries, std::uint64_t now, std::size_t keep, UsedAt usedAt) {
  std::vector<typename Map::key_type> doomed;
  // Idle rows are a part of all rows, so a cache under the cap has nothing to drop.
  if (entries.size() <= keep) {
    return doomed;
  }
  std::vector<std::pair<std::uint64_t, const typename Map::key_type*>> idle;
  for (const auto& entry : entries) {
    std::uint64_t used = usedAt(entry.second);
    if (used != now) {
      idle.emplace_back(used, &entry.first);
    }
  }
  if (idle.size() <= keep) {
    return doomed;
  }
  // Only which rows are the newest matters, not their order, so partition instead of sorting.
  std::nth_element(
    idle.begin(),
    idle.begin() + static_cast<std::ptrdiff_t>(keep),
    idle.end(),
    [](const auto& left, const auto& right) { return left.first > right.first; });
  doomed.reserve(idle.size() - keep);
  for (std::size_t index = keep; index < idle.size(); ++index) {
    doomed.push_back(*idle[index].second);
  }
  return doomed;
}

}
