#pragma once

#include <shadowlist-core/Constants.hpp>
#include <shadowlist-core/host/KeyDiff.hpp>

#include <cstddef>
#include <cstdint>
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
std::vector<std::size_t> insertionPositions(std::vector<std::size_t> indices, std::size_t previousCount);

/*
 * Positions of rows deleted from the old data: sorted, each one once. Positions past the end
 * name no row and are dropped.
 */
std::vector<std::size_t> deletionPositions(std::vector<std::size_t> indices, std::size_t previousCount);

/*
 * Where two key lists differ: the keys between the unchanged rows at both ends. The removed
 * keys run from start in the previous list, the added keys from start in the next one. An edit
 * at one end, like a prepend, is one splice the core applies without comparing every key.
 */
struct KeySplice {
  std::size_t start = 0;
  std::size_t removed = 0;
  std::size_t added = 0;

  bool isEmpty() const {
    return removed == 0 && added == 0;
  }
};

KeySplice keySplice(const std::vector<std::string>& previous, const std::vector<std::string>& next);

/*
 * The keys of the given rows. Rows past the end are skipped.
 */
std::unordered_set<std::string> keysOfRows(const std::vector<std::string>& keys, const std::vector<std::size_t>& rows);

/*
 * The rows whose key is in wanted, low to high.
 */
std::vector<std::size_t> rowsOfKeys(const std::vector<std::string>& keys, const std::unordered_set<std::string>& wanted);

/*
 * The keys after a planned batch. A kept row takes its previous key and an inserted row reads
 * its own through readKey, called with its index in the next data.
 */
template <typename ReadKey>
std::vector<std::string> keysFromPlan(const BatchPlan& plan, const std::vector<std::string>& previous, ReadKey readKey) {
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
 * The content version of every item by key, which applyChanges compares against to find the
 * items whose content changed under the same key.
 */
class ContentVersions final {
public:
  /*
   * Forget every version and keep these. versions has one value per item key.
   */
  void record(const std::vector<std::string>& itemKeys, const std::vector<std::int64_t>& versions);

  /*
   * The items whose version differs from the one kept for their key, low to high. Items
   * without a kept version did not change. The new versions are kept after.
   */
  std::vector<std::size_t> update(const std::vector<std::string>& itemKeys, const std::vector<std::int64_t>& versions);

  bool isEmpty() const { return versions_.empty(); }

private:
  std::unordered_map<std::string, std::int64_t> versions_;
};

}
