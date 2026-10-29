#pragma once

#include <shadowlist-core/KeyList.hpp>
#include <shadowlist-core/host/KeyDiff.hpp>
#include <shadowlist-core/host/SharedKeys.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Data changes a native list reads from its data source before they reach the core. Both kits
 * use these, which keeps one rule for every change.
 */

/*
 * Positions of rows inserted into the new data: sorted, each one once, and inside the new
 * data. A position past the end goes at the end: the k-th insert of the change lands at most at
 * previousCount + k. Hosts read the new keys at these positions.
 */
std::vector<std::size_t> insertionIndices(std::vector<std::size_t> indices, std::size_t previousCount);

/*
 * Positions of rows deleted from the old data: sorted, each one once. Positions past the end
 * name no row and are dropped.
 */
std::vector<std::size_t> deletionIndices(std::vector<std::size_t> indices, std::size_t previousCount);

/*
 * Where two key lists differ: the keys between the unchanged rows at both ends. The deleted
 * keys run from start in the previous list, the inserted keys from start in the next one. An edit
 * at one end, like a prepend, is one splice the core applies without comparing every key.
 */
struct KeySplice {
  std::size_t start = 0;
  std::size_t deleted = 0;
  std::size_t inserted = 0;

  bool isEmpty() const {
    return deleted == 0 && inserted == 0;
  }
};

KeySplice keySplice(const std::vector<std::string>& previous, const std::vector<std::string>& next);

/*
 * The keys of the given rows. Rows past the end are skipped.
 */
std::unordered_set<std::string> keysOfRows(
  const std::vector<std::string>& keys,
  const std::vector<std::size_t>& rows);

/*
 * The rows whose key is in wanted, low to high.
 */
std::vector<std::size_t> rowsOfKeys(
  const std::vector<std::string>& keys,
  const std::unordered_set<std::string>& wanted);

/*
 * The keys after a planned batch. A kept row takes its previous key and an inserted row reads
 * its own through readKey, called with its index in the next data.
 */
template <typename ReadKey>
std::vector<std::string> keysFromPlan(
  const BatchPlan& plan,
  const std::vector<std::string>& previous,
  ReadKey readKey) {
  std::vector<std::string> next;
  next.reserve(plan.sources.size());
  for (std::size_t index = 0; index < plan.sources.size(); ++index) {
    std::size_t source = plan.sources[index];
    if (source < previous.size()) {
      next.push_back(previous[source]);
    } else {
      next.push_back(readKey(index));
    }
  }
  return next;
}

/*
 * A batch performBatchUpdates collected, see BatchCollector. needsReload is a reloadData inside
 * the batch, after which the host reads everything again.
 */
struct CollectedBatch {
  BatchUpdate update;
  bool needsReload = false;

  bool isEmpty() const {
    return update.isEmpty() && !needsReload;
  }
};

/*
 * The changes a native list's performBatchUpdates collects until its outermost block returns.
 * Inserts, deletes, moves and reloads inside the block land here instead of reaching the data.
 * Hosts read their payloads themselves. Every kit keeps one beside its core, and it outlives a
 * core the kit drops and makes again.
 */
class BatchCollector final {
public:
  /*
   * A block starts. Blocks nest.
   */
  void begin() { ++depth_; }

  /*
   * A block returned. Returns whether it was the outermost, after which the host takes the batch.
   */
  bool end();

  /*
   * Whether a block is running, which is when changes go here.
   */
  bool isCollecting() const { return depth_ > 0; }

  void addDeleted(const std::vector<std::size_t>& indices);
  void addInserted(const std::vector<std::size_t>& indices);
  void addMoved(std::size_t from, std::size_t to);
  void addReloaded(const std::vector<std::size_t>& indices);
  void markNeedsReload() { batch_.needsReload = true; }

  /*
   * The changes collected, leaving none.
   */
  CollectedBatch take();

private:
  std::size_t depth_ = 0;
  CollectedBatch batch_;
};

/*
 * The content version of every item by key, which applyChanges compares against to find the
 * items whose content changed under the same key. Versions are int64. Hosts whose data sources
 * may answer something else map it to one: the wasm kit hashes a string version to 64 bits, see
 * versionHalves in its ShadowListKitCore.ts, and a change then goes unseen only if both versions
 * hash the same.
 */
class ContentVersions final {
public:
  /*
   * Forget every version and keep these. versions has one value per item key.
   */
  void record(const std::vector<std::string>& itemKeys, const std::vector<std::int64_t>& versions);
  void record(KeyList itemKeys, const std::vector<std::int64_t>& versions);

  /*
   * The items whose version differs from the one kept for their key, low to high. Items
   * without a kept version did not change. The new versions are kept after.
   */
  std::vector<std::size_t> update(const std::vector<std::string>& itemKeys, const std::vector<std::int64_t>& versions);
  std::vector<std::size_t> update(KeyList itemKeys, const std::vector<std::int64_t>& versions);

  bool isEmpty() const { return versions_.empty(); }

private:
  /*
   * A kept version and the round that last saw its key. Entries are updated in place, and the
   * keys a round did not see are swept after it. Data with the same keys allocates nothing.
   */
  struct Entry {
    std::int64_t version = 0;
    std::uint64_t round = 0;
  };

  /*
   * Keep versions as a new round. Calls changed with each item whose kept version differs.
   */
  template <typename Changed>
  void keep(KeyList itemKeys, const std::vector<std::int64_t>& versions, Changed changed);

  std::unordered_map<std::string, Entry> versions_;
  std::uint64_t round_ = 0;
};

/*
 * One edit of a key list: the keys between start unchanged keys at the front and end unchanged
 * keys at the back become inserted.
 */
struct KeyChange {
  std::size_t start = 0;
  std::size_t end = 0;
  std::vector<std::string> inserted;
};

/*
 * What HeldKeys::commit did: the splice of the rows, and the diff of the items when asked for,
 * with indices in the whole lists.
 */
struct HeldKeysCommit {
  KeySplice rows;
  KeyDiff diff;
};

/*
 * The row keys a native list holds across the cores it makes and drops, and the item keys of a
 * list with sections. Without sections the items are the rows. A change brings only the keys
 * between the unchanged ends, and the keys kept are shared with the previous version, see
 * SharedKeys. The selection and the content versions read the keys here, which then never cross
 * from the host again.
 */
class HeldKeys final {
public:
  KeyList rows() const { return rows_.list(); }
  KeyList items() const { return sectioned_ ? items_.list() : rows_.list(); }

  /*
   * Take the next keys. items is the change of a list with sections, nothing without. A start
   * or end past the keys held is clipped.
   */
  HeldKeysCommit commit(KeyChange rows, std::optional<KeyChange> items, bool diff);

  /*
   * Rows inserted at positions of the next keys, as insertionIndices gives them, in a list
   * without sections. keys has one key per position.
   */
  void insertRows(const std::vector<std::size_t>& indices, std::vector<std::string> keys);

  /*
   * Rows deleted at positions of the previous keys in a list without sections. deletionIndices
   * sorts them first.
   */
  void deleteRows(const std::vector<std::size_t>& indices);

private:
  SharedKeys rows_;
  SharedKeys items_;
  bool sectioned_ = false;
};

}
