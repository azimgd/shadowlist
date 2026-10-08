#pragma once

#include <shadowlist-core/Constants.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace azimgd::shadowlist {

/*
 * A row that stayed but changed its place: from is its index in the previous data, to in the
 * next.
 */
struct KeyMove {
  std::size_t from = 0;
  std::size_t to = 0;
};

/*
 * How one list of keys became another. Deleted are indices in the previous list, inserted
 * indices in the next one, both ascending. Moved are the kept keys outside the longest run that
 * kept its order, the fewest moves that explain the change. A key repeated in a list counts once,
 * at its first place. Its other places are deleted or inserted.
 */
struct KeyDiff {
  std::vector<std::size_t> deleted;
  std::vector<std::size_t> inserted;
  std::vector<KeyMove> moved;

  bool isEmpty() const {
    return deleted.empty() && inserted.empty() && moved.empty();
  }
};

KeyDiff diffKeys(const std::vector<std::string>& previous, const std::vector<std::string>& next);

/*
 * Changes collected in one batch the way UIKit takes them. Deleted, reloaded and the move
 * sources are indices in the previous data. Inserted and the move destinations are indices in
 * the next data.
 */
struct BatchUpdate {
  std::vector<std::size_t> deleted;
  std::vector<std::size_t> inserted;
  std::vector<KeyMove> moved;
  std::vector<std::size_t> reloaded;

  bool isEmpty() const {
    return deleted.empty() && inserted.empty() && moved.empty() && reloaded.empty();
  }
};

/*
 * Where every row of the next data comes from: the index of the previous row it keeps, or
 * UNDEFINED_INDEX for an inserted row whose key the host reads from its data.
 */
struct BatchPlan {
  std::vector<std::size_t> sources;
};

/*
 * Plan a batch. Deletes and move sources leave the previous rows, inserts and move
 * destinations fill their places in the next rows, and the rest keep their order. Returns
 * nothing for a batch that does not add up to nextCount rows, or that names a row twice. The
 * host then reloads everything instead.
 */
std::optional<BatchPlan> planBatch(std::size_t previousCount, std::size_t nextCount, const BatchUpdate& batch);

}
