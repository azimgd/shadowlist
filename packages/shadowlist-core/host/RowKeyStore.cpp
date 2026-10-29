#include <shadowlist-core/host/RowKeyStore.hpp>

#include <algorithm>
#include <utility>

namespace azimgd::shadowlist {

KeyEdit keyEditOf(std::size_t size, std::size_t start, std::size_t deleted, std::size_t inserted) {
  if (size == 0 || start > size || deleted > size - start || deleted == size || (deleted == 0 && inserted == 0)) {
    return {};
  }
  if (deleted == 0 && (start == 0 || start == size)) {
    return {start == 0 ? KeyEditKind::Prepend : KeyEditKind::Append, inserted};
  }
  if (inserted == 0 && (start == 0 || start + deleted == size)) {
    return {start == 0 ? KeyEditKind::TrimStart : KeyEditKind::TrimEnd, deleted};
  }
  return {KeyEditKind::Splice, 0, start, deleted, inserted};
}

std::size_t RowKeysSnapshot::indexOfKey(const std::string& key, std::size_t hint) const {
  KeyList rows = keys();
  if (hint != UNDEFINED_INDEX && !rows.empty()) {
    std::size_t center = std::min(hint, rows.size() - 1);
    if (rows[center] == key) {
      return center;
    }
    for (std::size_t distance = 1; distance <= HINT_WINDOW; ++distance) {
      bool inside = false;
      if (center >= distance) {
        inside = true;
        if (rows[center - distance] == key) {
          return center - distance;
        }
      }
      if (center + distance < rows.size()) {
        inside = true;
        if (rows[center + distance] == key) {
          return center + distance;
        }
      }
      if (!inside) {
        break;
      }
    }
  }
  std::call_once(indexOnce_, [this, &rows] {
    index_.reserve(rows.size());
    for (std::size_t row = 0; row < rows.size(); ++row) {
      index_.emplace(std::string_view(rows[row]), row);
    }
  });
  auto found = index_.find(std::string_view(key));
  return found == index_.end() ? UNDEFINED_INDEX : found->second;
}

template <typename T>
void RowKeyStore::Versions<T>::put(std::uint64_t version, std::shared_ptr<const T> value) {
  entries[version] = std::move(value);
  // Renders that never committed: keep the newest few past the committed version.
  std::size_t pending = 0;
  for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry) {
    if (entry->first <= adopted) {
      break;
    }
    ++pending;
  }
  while (pending > MAX_PENDING_VERSIONS) {
    auto oldest = entries.upper_bound(adopted);
    if (oldest == entries.end()) {
      break;
    }
    entries.erase(oldest);
    --pending;
  }
}

template <typename T>
std::shared_ptr<const T> RowKeyStore::Versions<T>::find(std::uint64_t version) const {
  auto entry = entries.find(version);
  return entry == entries.end() ? nullptr : entry->second;
}

template <typename T>
void RowKeyStore::Versions<T>::adopt(std::uint64_t version) {
  if (version == adopted) {
    return;
  }
  /*
   * Keep the version committed before this one too. Fabric can still adopt a clone of the
   * previous commit while this one lands.
   */
  previousAdopted = adopted;
  adopted = version;
  std::uint64_t keepFrom = std::min(previousAdopted, adopted);
  entries.erase(entries.begin(), entries.lower_bound(keepFrom));
}

bool RowKeyStore::publish(std::shared_ptr<RowKeysSnapshot> snapshot) {
  if (snapshot->version_ == 0) {
    return false;
  }
  // put's arguments convert the snapshot pointer, in an order the compiler picks.
  std::uint64_t version = snapshot->version_;
  keys_.put(version, std::move(snapshot));
  return true;
}

std::shared_ptr<const RowKeysSnapshot> RowKeyStore::baseOf(std::uint64_t baseVersion, RowKeysSnapshot::Kind kind) const {
  auto base = keys_.find(baseVersion);
  return base && base->kind_ == kind ? base : nullptr;
}

bool RowKeyStore::setKeys(std::uint64_t version, std::vector<std::string> keys) {
  auto snapshot = std::make_shared<RowKeysSnapshot>();
  snapshot->version_ = version;
  snapshot->keys_ = SharedKeys(std::move(keys));
  std::lock_guard<std::mutex> lock(mutex_);
  return publish(std::move(snapshot));
}

bool RowKeyStore::spliceKeys(
  std::uint64_t version,
  std::uint64_t baseVersion,
  std::size_t start,
  std::size_t deleteCount,
  std::vector<std::string> inserted) {
  std::shared_ptr<const RowKeysSnapshot> base;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    base = baseOf(baseVersion, RowKeysSnapshot::Kind::Plain);
  }
  if (!base || start > base->keys_.size() || deleteCount > base->keys_.size() - start) {
    return false;
  }
  auto snapshot = std::make_shared<RowKeysSnapshot>();
  snapshot->version_ = version;
  snapshot->baseVersion_ = baseVersion;
  snapshot->edit_ = keyEditOf(base->keys_.size(), start, deleteCount, inserted.size());
  snapshot->keys_ = base->keys_.splice(start, deleteCount, std::move(inserted));
  std::lock_guard<std::mutex> lock(mutex_);
  return publish(std::move(snapshot));
}

bool RowKeyStore::setSections(std::uint64_t version, std::uint64_t baseVersion, SectionsUpdate update) {
  std::size_t count = update.sectionKeys.size();
  if (update.itemCounts.size() != count || update.reuse.size() != count) {
    return false;
  }
  std::shared_ptr<const RowKeysSnapshot> base;
  if (baseVersion != 0) {
    std::lock_guard<std::mutex> lock(mutex_);
    base = baseOf(baseVersion, RowKeysSnapshot::Kind::Sections);
  }
  auto snapshot = std::make_shared<RowKeysSnapshot>();
  snapshot->version_ = version;
  snapshot->kind_ = RowKeysSnapshot::Kind::Sections;
  snapshot->qualifiedItems_ = update.qualifyItems;

  std::vector<SectionSpec> specs(count);
  for (std::size_t section = 0; section < count; ++section) {
    specs[section] = {update.itemCounts[section], update.hasHeader, update.hasFooter};
  }
  snapshot->sections_.setSections(std::move(specs));
  const ListSections& sections = snapshot->sections_;
  SharedKeys::Builder keys(base ? base->keys_ : SharedKeys(), sections.getRowCount());
  std::vector<std::string> sectionRows;

  std::size_t nextItemKey = 0;
  for (std::size_t section = 0; section < count; ++section) {
    std::size_t from = update.reuse[section];
    std::size_t rows = sections.rowCountInSection(section);
    if (from != UNDEFINED_INDEX) {
      if (!base || base->qualifiedItems_ != update.qualifyItems || from >= base->sections_.getSectionCount() ||
          base->sections_.rowCountInSection(from) != rows || base->sectionKeys_[from] != update.sectionKeys[section]) {
        return false;
      }
      keys.append(base->sections_.rowStartOfSection(from), rows);
      continue;
    }
    std::size_t items = update.itemCounts[section];
    if (nextItemKey + items > update.itemKeys.size()) {
      return false;
    }
    sectionRows.clear();
    sections.appendSectionRowKeys(
      section, update.sectionKeys[section], update.itemKeys.data() + nextItemKey, update.qualifyItems, sectionRows);
    for (std::string& key : sectionRows) {
      keys.append(std::move(key));
    }
    nextItemKey += items;
  }
  snapshot->keys_ = keys.finish();
  if (update.stickyHeaders && update.hasHeader) {
    snapshot->stickyRows_ = sections.headerRows();
  }
  snapshot->sectionKeys_ = std::move(update.sectionKeys);
  std::lock_guard<std::mutex> lock(mutex_);
  return publish(std::move(snapshot));
}

bool RowKeyStore::resetTree(std::uint64_t version, std::vector<TreeRow> rows) {
  auto snapshot = std::make_shared<RowKeysSnapshot>();
  snapshot->version_ = version;
  snapshot->kind_ = RowKeysSnapshot::Kind::Tree;
  snapshot->tree_.reset(std::move(rows));
  std::lock_guard<std::mutex> lock(mutex_);
  return publish(std::move(snapshot));
}

std::optional<TreeSplice> RowKeyStore::expandTree(
  std::uint64_t version,
  std::uint64_t baseVersion,
  std::size_t row,
  std::vector<TreeRow> descendants) {
  std::shared_ptr<const RowKeysSnapshot> base;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    base = baseOf(baseVersion, RowKeysSnapshot::Kind::Tree);
  }
  if (!base || row >= base->tree_.size()) {
    return std::nullopt;
  }
  auto snapshot = std::make_shared<RowKeysSnapshot>();
  snapshot->version_ = version;
  snapshot->baseVersion_ = baseVersion;
  snapshot->kind_ = RowKeysSnapshot::Kind::Tree;
  TreeSplice splice = snapshot->tree_.expandFrom(base->tree_, row, std::move(descendants));
  std::lock_guard<std::mutex> lock(mutex_);
  publish(std::move(snapshot));
  return splice;
}

std::optional<TreeSplice> RowKeyStore::collapseTree(std::uint64_t version, std::uint64_t baseVersion, std::size_t row) {
  std::shared_ptr<const RowKeysSnapshot> base;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    base = baseOf(baseVersion, RowKeysSnapshot::Kind::Tree);
  }
  if (!base || row >= base->tree_.size()) {
    return std::nullopt;
  }
  auto snapshot = std::make_shared<RowKeysSnapshot>();
  snapshot->version_ = version;
  snapshot->baseVersion_ = baseVersion;
  snapshot->kind_ = RowKeysSnapshot::Kind::Tree;
  TreeSplice splice = snapshot->tree_.collapseFrom(base->tree_, row);
  std::lock_guard<std::mutex> lock(mutex_);
  publish(std::move(snapshot));
  return splice;
}

std::shared_ptr<const RowKeysSnapshot> RowKeyStore::snapshot(std::uint64_t version) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return keys_.find(version);
}

void RowKeyStore::adopted(std::uint64_t version) {
  std::lock_guard<std::mutex> lock(mutex_);
  keys_.adopt(version);
}

void RowKeyStore::setSizeSpecs(std::uint64_t version, std::vector<RowSizeSpec> specs) {
  auto value = std::make_shared<const std::vector<RowSizeSpec>>(std::move(specs));
  std::lock_guard<std::mutex> lock(mutex_);
  specs_.put(version, std::move(value));
}

std::shared_ptr<const std::vector<RowSizeSpec>> RowKeyStore::sizeSpecs(std::uint64_t version) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return specs_.find(version);
}

void RowKeyStore::adoptedSizeSpecs(std::uint64_t version) {
  std::lock_guard<std::mutex> lock(mutex_);
  specs_.adopt(version);
}

std::size_t RowKeyStore::versionCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return keys_.entries.size();
}

}
