#include <shadowlist-core/Virtualizer.hpp>

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace azimgd::shadowlist {

namespace {
/*
 * Make room for size keys in the key map without rehashing it on every small change.
 * reserve with the exact size rehashes every row each time the list grows by a few rows.
 * Growing to twice the size keeps that amortized like the map's own growth.
 */
void reserveKeyMap(Revision& revision, std::size_t size) {
  auto& map = revision.elementIndexByKey;
  if (static_cast<double>(size) > static_cast<double>(map.bucket_count()) * map.max_load_factor()) {
    map.reserve(size * 2);
  }
}

/*
 * Whether keys from first on match the rows from rowFirst on, for count rows.
 */
bool keysMatchRows(
  const std::vector<Element>& rows,
  std::size_t rowFirst,
  const std::vector<std::string>& keys,
  std::size_t first,
  std::size_t count) {
  for (std::size_t position = 0; position < count; ++position) {
    if (rows[rowFirst + position].key != keys[first + position]) {
      return false;
    }
  }
  return true;
}

/*
 * The same match for an edit the host vouched for. Only the first and last row are checked,
 * which catches an edit applied at the wrong end or with the wrong count.
 */
bool keysMatchRows(
  const std::vector<Element>& rows,
  std::size_t rowFirst,
  const std::vector<std::string>& keys,
  std::size_t first,
  std::size_t count,
  bool vouched) {
  if (!vouched || count < 2) {
    return keysMatchRows(rows, rowFirst, keys, first, count);
  }
  return rows[rowFirst].key == keys[first] && rows[rowFirst + count - 1].key == keys[first + count - 1];
}

/*
 * Whether the host vouched for this edit with this many keys added or removed.
 */
bool vouchedFor(const KeyEdit& edit, KeyEditKind kind, std::size_t count) {
  return edit.kind == kind && edit.count == count;
}

/*
 * Drop the key map entries of rows first to first + count, before the rows go away.
 * Only valid without duplicate keys, where every row owns its entry.
 */
void eraseRowKeys(Revision& revision, std::size_t first, std::size_t count) {
  for (std::size_t index = first; index < first + count; ++index) {
    revision.elementIndexByKey.erase(revision.elements[index].key);
  }
}

/*
 * The new keys are the old rows with rows added at the end. Every old index holds.
 * Returns whether it applied.
 */
bool reconcileAppend(Container& container, const std::vector<std::string>& nextKeys, const KeyEdit& edit) {
  Revision& revision = container.revision;
  std::vector<Element>& rows = revision.elements;
  if (rows.empty() || nextKeys.size() <= rows.size()) {
    return false;
  }
  bool vouched = vouchedFor(edit, KeyEditKind::Append, nextKeys.size() - rows.size());
  if (!keysMatchRows(rows, 0, nextKeys, 0, rows.size(), vouched)) {
    return false;
  }
  // No exact reserve here. It would move every row on each small append.
  reserveKeyMap(revision, nextKeys.size());
  for (std::size_t index = rows.size(); index < nextKeys.size(); ++index) {
    rows.emplace_back();
    rows.back().key = nextKeys[index];
    rows.back().index = index;
    // A duplicate key keeps its first index, same as the general path.
    if (!revision.setIndexForKey(nextKeys[index], index)) {
      revision.hasDuplicateKeys = true;
    }
  }
  return true;
}

/*
 * The new keys are the old rows with new keys added at the start, none of them already
 * present. Every old index grows by the same count, which the bias absorbs.
 */
bool reconcilePrepend(Container& container, const std::vector<std::string>& nextKeys, const KeyEdit& edit) {
  Revision& revision = container.revision;
  std::vector<Element>& rows = revision.elements;
  if (rows.empty() || nextKeys.size() <= rows.size()) {
    return false;
  }
  std::size_t prependCount = nextKeys.size() - rows.size();
  bool vouched = vouchedFor(edit, KeyEditKind::Prepend, prependCount);
  if (!keysMatchRows(rows, 0, nextKeys, prependCount, rows.size(), vouched)) {
    return false;
  }
  // A prepended key that already exists takes the slow path. It is rare.
  for (std::size_t index = 0; index < prependCount; ++index) {
    if (revision.indexForKey(nextKeys[index]) != UNDEFINED_INDEX) {
      return false;
    }
  }
  revision.indexBias -= prependCount;
  rows.insert(rows.begin(), prependCount, Element{});
  reserveKeyMap(revision, nextKeys.size());
  for (std::size_t index = 0; index < prependCount; ++index) {
    rows[index].key = nextKeys[index];
    if (!revision.setIndexForKey(nextKeys[index], index)) {
      revision.hasDuplicateKeys = true;
    }
  }
  return true;
}

/*
 * The new keys are the old rows with rows removed from the end. Survivors keep their index.
 */
bool reconcileTrimEnd(Container& container, const std::vector<std::string>& nextKeys, const KeyEdit& edit) {
  Revision& revision = container.revision;
  std::vector<Element>& rows = revision.elements;
  if (revision.hasDuplicateKeys || nextKeys.empty() || nextKeys.size() >= rows.size()) {
    return false;
  }
  bool vouched = vouchedFor(edit, KeyEditKind::TrimEnd, rows.size() - nextKeys.size());
  if (!keysMatchRows(rows, 0, nextKeys, 0, nextKeys.size(), vouched)) {
    return false;
  }
  eraseRowKeys(revision, nextKeys.size(), rows.size() - nextKeys.size());
  rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(nextKeys.size()), rows.end());
  return true;
}

/*
 * The new keys are the old rows with rows removed from the start. Every survivor's index
 * drops by the same count, which the bias absorbs.
 */
bool reconcileTrimStart(Container& container, const std::vector<std::string>& nextKeys, const KeyEdit& edit) {
  Revision& revision = container.revision;
  std::vector<Element>& rows = revision.elements;
  if (revision.hasDuplicateKeys || nextKeys.empty() || nextKeys.size() >= rows.size()) {
    return false;
  }
  std::size_t trimCount = rows.size() - nextKeys.size();
  bool vouched = vouchedFor(edit, KeyEditKind::TrimStart, trimCount);
  if (!keysMatchRows(rows, trimCount, nextKeys, 0, nextKeys.size(), vouched)) {
    return false;
  }
  eraseRowKeys(revision, 0, trimCount);
  rows.erase(rows.begin(), rows.begin() + static_cast<std::ptrdiff_t>(trimCount));
  revision.indexBias += trimCount;
  return true;
}

/*
 * Match every new key to its old row through a new key map, built alongside the new rows.
 * Duplicate keys need this: only the first copy of a key takes over the old row.
 * Returns how many old rows survived.
 */
std::size_t reconcileRebuilding(Container& container, const std::vector<std::string>& nextKeys) {
  std::vector<Element>& previousElements = container.revision.elements;

  /*
   * Find surviving rows through the key map the last reconcile built, instead of a
   * throwaway copy. It stays in step with the rows, and the key check below makes sure.
   */
  const std::unordered_map<std::string, std::size_t>& previousIndexByKey =
    container.revision.elementIndexByKey;

  std::vector<Element> nextElements;
  nextElements.reserve(nextKeys.size());

  // Rebuild the key map with the rows so anchor lookups stay fast. Duplicates keep their first index.
  std::unordered_map<std::string, std::size_t> nextElementIndexByKey;
  nextElementIndexByKey.reserve(nextKeys.size());

  /*
   * Old map values still carry the old bias. Only reset it after the last read below,
   * or every lookup in this loop finds the wrong row.
   */
  const std::size_t previousIndexBias = container.revision.indexBias;

  std::size_t survivorCount = 0;
  bool duplicates = false;

  for (std::size_t nextElementIndex = 0; nextElementIndex < nextKeys.size(); ++nextElementIndex) {
    const std::string& nextKey = nextKeys[nextElementIndex];

    /*
     * Only the first copy of a key can take over the old row, since both maps keep the
     * first copy. A later duplicate gets a new row.
     */
    bool firstOccurrence = nextElementIndexByKey.emplace(nextKey, nextElementIndex).second;
    duplicates = duplicates || !firstOccurrence;

    auto previousElementEntry = nextKey.empty() ? previousIndexByKey.end() : previousIndexByKey.find(nextKey);
    std::size_t previousElementIndex = previousElementEntry != previousIndexByKey.end()
      ? previousElementEntry->second - previousIndexBias
      : UNDEFINED_INDEX;
    bool survives =
      firstOccurrence &&
      previousElementIndex < previousElements.size() &&
      previousElements[previousElementIndex].key == nextKey;

    if (survives) {
      // Keep the row's size and flags. Move it straight into place to avoid a second copy.
      nextElements.push_back(std::move(previousElements[previousElementIndex]));
      nextElements.back().index = nextElementIndex;
      survivorCount++;
    } else {
      nextElements.emplace_back();
      nextElements.back().key = nextKey;
      nextElements.back().index = nextElementIndex;
    }
  }

  container.revision.elements = std::move(nextElements);
  container.revision.elementIndexByKey = std::move(nextElementIndexByKey);
  // The rebuilt map holds true indices.
  container.revision.indexBias = 0;
  container.revision.hasDuplicateKeys = duplicates;
  return survivorCount;
}

/*
 * Match every new key to its old row and update the key map in place, without duplicate
 * keys in the old rows. Each old row then owns its entry. An entry this pass already moved
 * to a new index points at an earlier new row with the same key, which marks a duplicate
 * in the new keys. Old rows no key took over lose their entries at the end.
 * Returns how many old rows survived.
 */
std::size_t reconcileMatching(Container& container, const std::vector<std::string>& nextKeys) {
  Revision& revision = container.revision;
  std::vector<Element>& previousElements = revision.elements;
  auto& map = revision.elementIndexByKey;
  const std::size_t bias = revision.indexBias;
  std::vector<bool> taken(previousElements.size(), false);
  std::vector<Element> nextElements;
  nextElements.reserve(nextKeys.size());
  reserveKeyMap(revision, nextKeys.size());
  std::size_t survivorCount = 0;
  bool duplicates = false;

  for (std::size_t nextIndex = 0; nextIndex < nextKeys.size(); ++nextIndex) {
    const std::string& nextKey = nextKeys[nextIndex];
    auto entry = nextKey.empty() ? map.end() : map.find(nextKey);
    std::size_t stored = entry != map.end() ? entry->second - bias : UNDEFINED_INDEX;
    bool repeated = stored < nextIndex && nextKeys[stored] == nextKey;
    bool survives = entry != map.end() && !repeated && stored < previousElements.size() &&
      !taken[stored] && previousElements[stored].key == nextKey;
    if (survives) {
      taken[stored] = true;
      nextElements.push_back(std::move(previousElements[stored]));
      survivorCount++;
    } else {
      nextElements.emplace_back();
      nextElements.back().key = nextKey;
    }
    nextElements.back().index = nextIndex;
    duplicates = duplicates || repeated;
    if (repeated || nextKey.empty()) {
      continue;
    }
    if (entry != map.end()) {
      entry->second = nextIndex + bias;
    } else {
      map.emplace(nextKey, nextIndex + bias);
    }
  }

  for (std::size_t previousIndex = 0; previousIndex < previousElements.size(); ++previousIndex) {
    if (!taken[previousIndex] && !previousElements[previousIndex].key.empty()) {
      map.erase(previousElements[previousIndex].key);
    }
  }
  revision.elements = std::move(nextElements);
  revision.hasDuplicateKeys = duplicates;
  return survivorCount;
}
}

std::size_t Virtualizer::reconcileElements(
  Container& container,
  const std::vector<std::string>& nextKeys,
  KeyEdit edit) {
  std::lock_guard<std::recursive_mutex> lock(container.coreMutex);

  std::vector<Element>& previousElements = container.revision.elements;

  /*
   * Fast paths for edits at either end, like an append, loading older chat messages or
   * trimming a window of rows. Rebuilding the key map costs about 6ms at 100k rows, a dropped
   * frame. These keep the rows and the map in place. An empty list is a cold start and takes
   * the general path.
   * Rows are not renumbered here. The reflow from row 0 sets every index anyway, and nothing
   * reads the index before then.
   */
  std::size_t previousCount = previousElements.size();
  bool grew = reconcileAppend(container, nextKeys, edit) || reconcilePrepend(container, nextKeys, edit);
  bool shrank = !grew && (reconcileTrimEnd(container, nextKeys, edit) || reconcileTrimStart(container, nextKeys, edit));
  if (grew || shrank) {
    container.geometryVersion++;
    container.elementsStructureDirty = true;
    return std::min(previousCount, nextKeys.size());
  }
  /*
   * Any other change matches every new key to its old row. Without duplicate keys the key
   * map is updated in place. With them the map is rebuilt to find which copy takes a row.
   */
  std::size_t survivorCount = container.revision.hasDuplicateKeys
    ? reconcileRebuilding(container, nextKeys)
    : reconcileMatching(container, nextKeys);

  // Rows moved. Any geometry derived from the old list is stale.
  container.geometryVersion++;

  // Rows were added, removed or moved. The next layout must recompute offsets.
  container.elementsStructureDirty = true;

  /*
   * No row survived. The whole dataset was swapped. Reset the average so it is taken
   * again from the new rows. If some rows survived, the old average still fits.
   */
  if (survivorCount == 0) {
    container.revision.averageElementWidth = 0.0;
    container.revision.averageElementHeight = 0.0;
    container.revision.measuredRealCount = 0;
    container.revision.measuredRealTotalWidth = 0.0;
    container.revision.measuredRealTotalHeight = 0.0;
  }
  return survivorCount;
}

}
